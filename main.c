/* 					*
 * Name: 	Nadeem Anwar           	*
 * Email: 	ibnul.aftab@proton.me 	*
 * 					*/

// TODO(Nadeem Anwar): plan error handling
// TODO(Nadeem Anwar): replace exit with proper function that deallocates memory
// XXX: DON'T TRY TO MAKE ERROR HANDLING IF STATEMENTS INTO A MACRO. EXTREMELY BAD IDEA. PLEASE BEAR WITH IT.

#define _GNU_SOURCE

#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <wayland-client.h>
#include <sys/mman.h>
#include <unistd.h>

#include "xdg-shell-client-protocol.h"
#include "debug.h"
#include "render.h"

#define WIDTH 500
#define HEIGHT 600

enum FLAGS {DEFAULT, CREATE, RESIZE};

static struct frame {	
	struct wl_buffer * buffer;
	struct wl_callback * callback;
	uint32_t * pixels;
	uint32_t width;
	uint32_t height;
	uint32_t size;	
	uint32_t stride; 
	int fd;
	bool free;
} frame = {0};

static struct state {	
	struct wl_display * display;		
	struct wl_event_queue * display_queue;
	struct wl_event_queue * render_queue;
	struct wl_compositor * compositor;
	struct wl_shm * shared_memory;
	struct xdg_wm_base * window_manager_base;
	struct wl_surface * wl_surface;
	struct xdg_surface * xdg_surface;
	struct xdg_toplevel * xdg_toplevel;
	struct frame * frame;
	enum FLAGS flag;
	bool running;
} state = {
	.display = NULL,
	.display_queue = NULL,
	.compositor = NULL,
	.shared_memory = NULL,
	.window_manager_base = NULL,
	.wl_surface = NULL,
	.xdg_surface = NULL,
	.xdg_toplevel = NULL,
	.running = false,
	.flag = DEFAULT,

	.frame = &frame
};

static void registery_global(void * data, struct wl_registry * registery, uint32_t name, const char * interface, uint32_t version);
static void registery_global_remove(void * data, struct wl_registry * registery, uint32_t name);
static void ping (void *data, struct xdg_wm_base *xdg_wm_base, uint32_t serial);
static void xdg_surface_configure (void *data, struct xdg_surface *xdg_surface, uint32_t serial);
static void xdg_toplevel_close(void *data, struct xdg_toplevel *xdg_toplevel);
static void xdg_toplevel_configure (void *data, struct xdg_toplevel *xdg_toplevel, int32_t width, int32_t height, struct wl_array *states);
static void xdg_toplevel_configure_bounds (void *data, struct xdg_toplevel *xdg_toplevel, int32_t width, int32_t height);
static void xdg_toplevel_wm_capabilities (void *data, struct xdg_toplevel *xdg_toplevel, struct wl_array *capabilities);
static void wl_callback_frame_done(void *data, struct wl_callback *wl_callback, uint32_t callback_data);
static void wl_buffer_release (void *data, struct wl_buffer *wl_buffer);

static const struct xdg_wm_base_listener window_manager_base_listener = {
	.ping = &ping
};

static const struct xdg_surface_listener xdg_surface_listener = {
	.configure = &xdg_surface_configure 
};

static const struct xdg_toplevel_listener xdg_toplevel_listener = {
	.configure = &xdg_toplevel_configure,
	.close = &xdg_toplevel_close,
	.configure_bounds = xdg_toplevel_configure_bounds,
	.wm_capabilities = xdg_toplevel_wm_capabilities
};

static const struct wl_callback_listener wl_callback_listener = {
	.done = &wl_callback_frame_done
};


/* Summary:												*
 * Connects  a  wl_display which is a singleton  object representing client connection  to  wayland     *
 * compositor  used to find registeries advertised by compositors. (That's how I would like to  put     *
 * it.  We connect compositor (that is charge of combining multiple contents of surfaces),  surface     *
 * (recieves input and is used to attach buffers),  shared memory (to make buffer available to both     *
 * compositor  and client) and xdg window  manager base (provides basic functionality like  resize,     *
 * moving  and scalling). We fetch them by dispatching  the registery's queue. We create a  surface     *
 * (provides functionality of adding buffers, recieving inputs and having a local cordinate system)     *
 * 													*/

int main (void)
{
	state.display = wl_display_connect(NULL);
	
	if(! state.display)
	{
		print_log(FAIL, "Unable to initialize state.display from wl_display_connect()");
		exit(ERR_DISPLAY);
	}
	
	print_log(SUCCESS, "Initialized state.display from wl_display_connect()");	
	state.display_queue = wl_display_create_queue(state.display);

    	const struct wl_registry_listener registery_listener = {
		.global = &registery_global,
		.global_remove = &registery_global_remove
	};
	
	struct wl_registry * registry = wl_display_get_registry(state.display);
	struct wl_event_queue * registry_queue = wl_display_create_queue(state.display);
	wl_proxy_set_queue((struct wl_proxy *) registry, registry_queue);

	wl_registry_add_listener(registry, &registery_listener, NULL);
	
	wl_display_roundtrip_queue(state.display, registry_queue);
	print_log(SUCCESS, "Initialized state.compositor, state.shared_memory, state.window_manager_base in wl_display_roundtrip()");

	wl_registry_destroy(registry);
	wl_event_queue_destroy(registry_queue);

	xdg_wm_base_add_listener(state.window_manager_base, &window_manager_base_listener, NULL);

	state.wl_surface = wl_compositor_create_surface(state.compositor);

	if(! state.wl_surface)
	{
		print_log(FAIL, "Unable to initialize state.wl_surface from wl_compositor_create_surface()");
		exit(ERR_WL_SURFACE);
	}

	print_log(SUCCESS, "Initialized state.wl_surface from wl_display_connect()");

	state.xdg_surface = xdg_wm_base_get_xdg_surface(state.window_manager_base, state.wl_surface);
	
	if(! state.xdg_surface)
	{
		print_log(FAIL, "Unable to initialize state.wl_surface from xdg_wm_base_get_xdg_surface()");
		exit(ERR_XDG_SURFACE);
	}

	print_log(SUCCESS, "Initialized state.xdg_surface from xdg_wm_base_get_xdg_surface()");

	state.xdg_toplevel = xdg_surface_get_toplevel(state.xdg_surface);
	
	xdg_surface_add_listener(state.xdg_surface, &xdg_surface_listener, NULL);
	
	if(! state.xdg_toplevel)
	{
		print_log(FAIL, "Unable to initialize state.xdg_toplevel from xdg_surface_get_toplevel()");
		exit(ERR_XDG_SURFACE);
	}
	
	print_log(SUCCESS, "Initialized state.xdg_toplevel from xdg_surface_get_toplevel()");
	
	xdg_toplevel_add_listener(state.xdg_toplevel, &xdg_toplevel_listener, NULL);
	
	xdg_toplevel_set_title(state.xdg_toplevel, "Rotating Cube");

	wl_surface_commit(state.wl_surface);

	state.running = true;
	while(state.running && wl_display_dispatch_queue(state.display, state.display_queue) != -1);

	wl_buffer_destroy(frame.buffer);	
	wl_callback_destroy(frame.callback);
	
	xdg_toplevel_destroy(state.xdg_toplevel);
	xdg_surface_destroy(state.xdg_surface);
	wl_surface_destroy(state.wl_surface);

	xdg_wm_base_destroy(state.window_manager_base);
	wl_shm_destroy(state.shared_memory);
	wl_compositor_destroy(state.compositor);

	wl_display_disconnect(state.display);

	if(munmap(frame.pixels, frame.size) < 0)
	{
		print_log(FAIL, "Unable to free memory: frame of size: %d", frame.size);
		exit(ERR_MEM);
	}

	close(frame.fd);

	return 0;
}

static void registery_global(void * data, struct wl_registry * registery, uint32_t name, const char * interface, uint32_t version)
{
	print_log(LOG, "Found: %s", interface);
	
	if (! strcmp(interface, wl_compositor_interface.name))
	{
		state.compositor = wl_registry_bind(registery, name, &wl_compositor_interface, 4);
		
		if(! state.compositor)
		{
			print_log(FAIL, "Unable to initialize state.compositor from wl_registery_bind()");
			exit(ERR_COMPOSITOR);
		}
	
		print_log(SUCCESS, "Initialized state.compositor from wl_registery_bind()");
		wl_proxy_set_queue((struct wl_proxy *) state.compositor, state.display_queue);

		return;
	}

	if (! strcmp(interface, wl_shm_interface.name))
	{

		state.shared_memory = wl_registry_bind(registery, name, &wl_shm_interface, 1);
	
		if(! state.shared_memory)
		{
			print_log(FAIL, "Unable to initialize state.shared_memory from wl_registery_bind()");
			exit(ERR_SHM);
		}
	
		print_log(SUCCESS, "Initialized state.shared_memory from wl_registery_bind()");

		wl_proxy_set_queue((struct wl_proxy *) state.shared_memory, state.display_queue);
		
		return;
	}

	if (! strcmp(interface, xdg_wm_base_interface.name))
	{

		state.window_manager_base = wl_registry_bind(registery, name, &xdg_wm_base_interface, 6);

		if(! state.window_manager_base)
		{ 
			print_log(FAIL, "Unable to initialize state.window_manager_base from wl_registery_bind()");
			exit(ERR_XDG_WM_BASE);
		}
	
		print_log(SUCCESS, "Initialized state.window_manager_base from wl_registery_bind()");	
		wl_proxy_set_queue((struct wl_proxy *) state.window_manager_base, state.display_queue);
		
		return;
	}
}

static void registery_global_remove(void * data, struct wl_registry * registery, uint32_t name) 
{
	print_log(LOG, "Event dispatched...");	
}

void ping (void *data, struct xdg_wm_base *xdg_wm_base, uint32_t serial)
{
	print_log(LOG, "Event dispatched...");	
	
	xdg_wm_base_pong(xdg_wm_base, serial);
}

int allocate_shm_file(size_t size)
{
	int fd = memfd_create("main", MFD_CLOEXEC);
	
	if (fd < 0)
	{
		print_log(FAIL, "Unable to create anonymous file: main");
		
		exit(ERR_FILE);
	}
	
	print_log(SUCCESS, "Created anonymous file main");
	
	if(ftruncate(fd, size) != 0)
	{
		print_log(FAIL, "Invalid size of the anonymous file: main");
		close(fd);
		
		exit(ERR_FILE);
	}

	return fd;
}

// TODO(Nadeem Anwar): xdg_toplevel_configure: width == 0 and heingth == 0 not necisserily when the window is created 
void xdg_surface_configure (void *data, struct xdg_surface *xdg_surface, uint32_t serial)
{
	print_log(LOG, "Event dispatched...");
	
	xdg_surface_ack_configure(state.xdg_surface, serial);
	
	switch (state.flag)	
	{
		case CREATE:
			{

				print_log(LOG, "Creating frame buffer...");
			
				frame.stride = frame.width * 4;
				frame.size = frame.stride * frame.height;

				print_log(LOG, "Buffer size: %d bytes", frame.size);

				frame.fd = allocate_shm_file(frame.size);
				frame.pixels = mmap(NULL, frame.size, PROT_READ | PROT_WRITE, MAP_SHARED, frame.fd, 0);
				
				if(frame.pixels == MAP_FAILED)
				{
					print_log(FAIL, "Unable to map memory from the annonymous file to frame");
					close(frame.fd);

					exit(ERR_MEM);
				}

				print_log(SUCCESS, "Mapped memory from the annonymous file to the frame");
			
				memset(frame.pixels, 0x00, frame.size);

				struct wl_shm_pool * pool = wl_shm_create_pool(state.shared_memory, frame.fd, frame.size);
				frame.buffer = wl_shm_pool_create_buffer(pool, 0, frame.width, frame.height, frame.stride, WL_SHM_FORMAT_ARGB8888);
				wl_shm_pool_destroy(pool);

				print_log(LOG, "Created frame buffer: state.frame");

				static const struct wl_buffer_listener wl_buffer_listener = {
					.release = &wl_buffer_release
				};

				wl_buffer_add_listener(frame.buffer, &wl_buffer_listener, NULL);

				frame.free = true;
				
				print_log(LOG, "frame.free = %d", frame.free);
				
				frame.callback = wl_surface_frame(state.wl_surface);
				wl_callback_add_listener(frame.callback, &wl_callback_listener, data);

				wl_surface_attach(state.wl_surface, frame.buffer, 0, 0);
				wl_surface_commit(state.wl_surface);

				state.flag = DEFAULT;
				
				return;
			}

		default: break;
	}
		
	wl_callback_destroy(frame.callback);
	frame.callback = wl_surface_frame(state.wl_surface);
	wl_callback_add_listener(frame.callback, &wl_callback_listener, data);
	
	wl_surface_attach(state.wl_surface, frame.buffer, 0, 0);
	wl_surface_commit(state.wl_surface);
}

void xdg_toplevel_configure (void *data, struct xdg_toplevel *xdg_toplevel, int32_t width, int32_t height, struct wl_array *states)
{
	print_log(LOG, "Event dispatched...");
	print_log(LOG, "Width: %d; Height: %d", width, height);

	if(width == 0 && height == 0) 
	{
		frame.width = WIDTH;
		frame.height = HEIGHT;

		state.flag = CREATE;
	}
	else 
	{
		frame.width = width;
		frame.height = height;
	}
}

void wl_callback_frame_done(void *data, struct wl_callback *wl_callback, uint32_t callback_data)
{
	static uint32_t time = 0;
	if(time == 0)	time = callback_data;
	
	print_log(LOG, "delta callbacks = %d - %d = %d", callback_data, time, callback_data - time);
	
	time = callback_data;
	
	wl_callback_destroy(frame.callback);
	frame.callback = wl_surface_frame(state.wl_surface);
	wl_callback_add_listener(frame.callback, &wl_callback_listener, data);
	
	if(! frame.free)
	{	
		print_log(LOG, "frame.free = %d", frame.free);	
		return;
	}

	frame.free = false;
	print_log(LOG, "frame.free = %d", frame.free);

	draw(frame.pixels, frame.height, frame.width);

	wl_surface_attach(state.wl_surface, frame.buffer, 0, 0);
	wl_surface_damage_buffer(state.wl_surface, 0, 0, frame.width, frame.height);
	wl_surface_commit(state.wl_surface);
}

void xdg_toplevel_close(void *data, struct xdg_toplevel *xdg_toplevel)
{
	print_log(LOG, "Event dispatched...");

	state.running = false;
}

void xdg_toplevel_configure_bounds (void *data, struct xdg_toplevel *xdg_toplevel, int32_t width, int32_t height)
{
	print_log(LOG, "Event dispatched...");
}

void xdg_toplevel_wm_capabilities (void *data, struct xdg_toplevel *xdg_toplevel, struct wl_array *capabilities)
{
	print_log(LOG, "Event dispatched...");
}

void wl_buffer_release (void *data, struct wl_buffer *wl_buffer)
{
	print_log(LOG, "Event dispatched...");

	frame.free = true;
	print_log(LOG, "frame.free = %d", frame.free);
}
