/* 					*
 * Name: 	Lain			*
 * 					*/

// TODO: plan error handling
// TODO: replace exit with proper function that deallocates memory
// TODO: documentation
// XXX:  DON'T TRY TO MAKE ERROR HANDLING IF STATEMENTS INTO A MACRO. EXTREMELY BAD IDEA. PLEASE BEAR WITH IT.
// 	 Strips the abblity to write custom messages.	

#define _GNU_SOURCE

#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <wayland-client.h>
#include <sys/mman.h>
#include <unistd.h>
#include <threads.h>

#include "xdg-shell-client-protocol.h"
#include "errors.h"
#include "debug.h"
#include "render.h"

#define WIDTH 	800
#define HEIGHT 	800

#define VAR(x)	#x

enum FLAGS {DEFAULT, CREATE, RESIZE};

static struct frame {	
	struct wl_buffer * buffer;
	uint32_t * pixels;
	bool free;

	#ifdef DEBUG
	uint8_t id;
	#endif

} frame[2] = {{0}, {0}};

static struct metadata {	
	struct wl_callback * callback;
	uint32_t width;
	uint32_t height;
	uint32_t frame_size;
	uint32_t stride;
	uint32_t buffer_size;
	int fd;
	uint8_t * buffer;
	const uint8_t len;
} metadata = {
		.width = 0,
		.height = 0,
		.frame_size = 0,
		.stride = 0,
		.len = 2,
		.buffer = NULL
};

// TODO: merge state and metadata togather
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
	
	.flag = DEFAULT,
	.running = false,
};

static mtx_t mutex;

static void set_globals (void * data, struct wl_registry * registery, uint32_t name, const char * interface, uint32_t version);
static void handle_removed_globals (void * data, struct wl_registry * registery, uint32_t name);
static void ping (void *data, struct xdg_wm_base *xdg_wm_base, uint32_t serial);
static void configure_surface (void *data, struct xdg_surface *xdg_surface, uint32_t serial);
static void close_xdg_toplevel (void *data, struct xdg_toplevel *xdg_toplevel);
static void configure_toplevel (void *data, struct xdg_toplevel *xdg_toplevel, int32_t width, int32_t height, struct wl_array *states);
static void configure_toplevel_bounds (void *data, struct xdg_toplevel *xdg_toplevel, int32_t width, int32_t height);
static void set_wm_capabilities (void *data, struct xdg_toplevel *xdg_toplevel, struct wl_array *capabilities);
static void done_callback (void *data, struct wl_callback *wl_callback, uint32_t callback_data);
static void release_buffer (void *data, struct wl_buffer *wl_buffer);

int dispatch_display_queue(void * args);
int dispatch_render_queue(void * args);

static const struct xdg_wm_base_listener window_manager_base_listener = {
	.ping = &ping
};

static const struct xdg_surface_listener xdg_surface_listener = {
	.configure = &configure_surface  
};

static const struct xdg_toplevel_listener xdg_toplevel_listener = {
	.configure = &configure_toplevel,
	.close = &close_xdg_toplevel,
	.configure_bounds = configure_toplevel_bounds,
	.wm_capabilities = set_wm_capabilities
};

static const struct wl_callback_listener wl_callback_listener = {
	.done = &done_callback
};

static const struct wl_buffer_listener wl_buffer_listener = {
	.release = &release_buffer
};

int main (void)
{
	START_BENCHMARK(1);
	
	state.display = wl_display_connect(NULL);
	
	if(! state.display)
	{
		PRINT_LOG(FAIL, "Unable to initialize " BOLD "state.display" RESET " from " BOLD "wl_display_connect()" RESET);
		exit(ERR_DISPLAY);
	}
	
	PRINT_LOG(SUCCESS, "Initialized " BOLD "state.display" RESET " from " BOLD "wl_display_connect()" RESET);
	
	state.display_queue = wl_display_create_queue(state.display);

	struct wl_registry * registry = wl_display_get_registry(state.display);
	struct wl_event_queue * registry_queue = wl_display_create_queue(state.display);
	wl_proxy_set_queue((struct wl_proxy *) registry, registry_queue);

    	const struct wl_registry_listener registery_listener = {
		.global = &set_globals,
		.global_remove = &handle_removed_globals
	};

	wl_registry_add_listener(registry, &registery_listener, NULL);
	
	START_BENCHMARK(2);
	
	// Fetches available global objects
	wl_display_roundtrip_queue(state.display, registry_queue);	
	
	END_BENCHMARK(2, BOLD "wl_display_roundtrip_queue()" RESET);

	wl_registry_destroy(registry);
	wl_event_queue_destroy(registry_queue);
	
	PRINT_LOG(SUCCESS, "Initialized " BOLD "state.compositor, state.shared_memory, state.window_manager_base" RESET " in " BOLD "wl_display_roundtrip()" RESET);

	xdg_wm_base_add_listener(state.window_manager_base, &window_manager_base_listener, NULL);

	state.wl_surface = wl_compositor_create_surface(state.compositor);

	if(! state.wl_surface)
	{
		PRINT_LOG(FAIL, "Unable to initialize " BOLD "state.wl_surface" RESET " from " BOLD "wl_compositor_create_surface()" RESET);
		exit(ERR_WL_SURFACE);
	}

	PRINT_LOG(SUCCESS, "Initialized " BOLD "state.wl_surface" RESET " from " BOLD "wl_display_connect()" RESET);

	state.xdg_surface = xdg_wm_base_get_xdg_surface(state.window_manager_base, state.wl_surface);
	
	if(! state.xdg_surface)
	{
		PRINT_LOG(FAIL, "Unable to initialize " BOLD "state.wl_surface" RESET " from " BOLD "xdg_wm_base_get_xdg_surface()" RESET);
		exit(ERR_XDG_SURFACE);
	}

	PRINT_LOG(SUCCESS, "Initialized " BOLD "state.xdg_surface" RESET " from " BOLD "xdg_wm_base_get_xdg_surface()" RESET);

	xdg_surface_add_listener(state.xdg_surface, &xdg_surface_listener, NULL);
	
	state.xdg_toplevel = xdg_surface_get_toplevel(state.xdg_surface);
	
	if(! state.xdg_toplevel)
	{
		PRINT_LOG(FAIL, "Unable to initialize " BOLD "state.xdg_toplevel" RESET " from " BOLD "xdg_surface_get_toplevel()" RESET);
		exit(ERR_XDG_SURFACE);
	}
	
	PRINT_LOG(SUCCESS, "Initialized " BOLD "state.xdg_toplevel" RESET " from " BOLD "xdg_surface_get_toplevel()" RESET);
	
	xdg_toplevel_add_listener(state.xdg_toplevel, &xdg_toplevel_listener, NULL);
	xdg_toplevel_set_title(state.xdg_toplevel, "Wayland Client");

	wl_surface_commit(state.wl_surface);
	
	END_BENCHMARK(1, "before " BOLD "wl_display_dispatch_queue()" RESET);
	
	state.render_queue = wl_display_create_queue(state.display);

	state.running = true;
	
	thrd_t display_queue_thrd_id;
	thrd_create(&display_queue_thrd_id, dispatch_display_queue, NULL);
	int display_queue_thrd_ret_val;

	thrd_t render_queue_thrd_id;
	thrd_create(&render_queue_thrd_id, dispatch_render_queue, NULL);
	int render_queue_thrd_ret_val; 

	thrd_join(render_queue_thrd_id, &render_queue_thrd_ret_val);
	thrd_join(display_queue_thrd_id, &display_queue_thrd_ret_val);

	for(int i = 0; i < metadata.len; i++)	wl_buffer_destroy(frame[i].buffer);

	wl_callback_destroy(metadata.callback);
	xdg_toplevel_destroy(state.xdg_toplevel);
	xdg_surface_destroy(state.xdg_surface);
	wl_surface_destroy(state.wl_surface);

	xdg_wm_base_destroy(state.window_manager_base);
	wl_shm_destroy(state.shared_memory);
	wl_compositor_destroy(state.compositor);

	wl_display_disconnect(state.display);
	
	if(munmap(metadata.buffer, metadata.buffer_size) < 0)
	{
		PRINT_LOG(FAIL, "Unable to free memory: " BOLD VAR(metadata.buffer) RESET " having size = " BOLD "%d" RESET, metadata.buffer_size);
		exit(ERR_MEM);
	}
	
	close(metadata.fd);

	END_BENCHMARK(1, BOLD "%s()" RESET, __func__);
	
	return 0;
}

static void set_globals(void * data, struct wl_registry * registery, uint32_t name, const char * interface, uint32_t version)
{	
	PRINT_LOG(LOG, "Found: " BOLD "%s" RESET, interface);
	
	if (! strcmp(interface, wl_compositor_interface.name))
	{
		state.compositor = wl_registry_bind(registery, name, &wl_compositor_interface, 4);
		
		if(! state.compositor)
		{
			PRINT_LOG(FAIL, "Unable to initialize " BOLD "state.compositor" RESET " from " BOLD "wl_registery_bind()" RESET);
			exit(ERR_COMPOSITOR);
		}
	
		PRINT_LOG(SUCCESS, "Initialized " BOLD "state.compositor" RESET " from " BOLD "wl_registery_bind()" RESET, state.compositor);
		wl_proxy_set_queue((struct wl_proxy *) state.compositor, state.display_queue);
		
		return;
	}

	if (! strcmp(interface, wl_shm_interface.name))
	{

		state.shared_memory = wl_registry_bind(registery, name, &wl_shm_interface, 1);
	
		if(! state.shared_memory)
		{
			PRINT_LOG(FAIL, "Unable to initialize " BOLD "state.shared_memory" RESET " from " BOLD "wl_registery_bind()" RESET, state.shared_memory);
			exit(ERR_SHM);
		}
	
		PRINT_LOG(SUCCESS, "Initialized " BOLD "state.shared_memory" RESET " from " BOLD "wl_registery_bind()" RESET, state.shared_memory);

		wl_proxy_set_queue((struct wl_proxy *) state.shared_memory, state.display_queue);

		return;
	}

	if (! strcmp(interface, xdg_wm_base_interface.name))
	{

		state.window_manager_base = wl_registry_bind(registery, name, &xdg_wm_base_interface, 6);

		if(! state.window_manager_base)
		{ 
			PRINT_LOG(FAIL, "Unable to initialize " BOLD "state.window_manager_base" RESET " from " BOLD "wl_registery_bind()" RESET);
			exit(ERR_XDG_WM_BASE);
		}
	
		PRINT_LOG(SUCCESS, "Initialized " BOLD "state.window_manager_base" RESET " from " BOLD "wl_registery_bind()" RESET, state.window_manager_base);	
		wl_proxy_set_queue((struct wl_proxy *) state.window_manager_base, state.display_queue);
		
		return;
	}
}

static void handle_removed_globals(void * data, struct wl_registry * registery, uint32_t name) {}

static void ping (void *data, struct xdg_wm_base *xdg_wm_base, uint32_t serial)
{
	PRINT_LOG(LOG, "Application is active!");	
	
	xdg_wm_base_pong(xdg_wm_base, serial);
}

inline static int allocate_shm_file()
{
	char * const file_name = "frame-buffer";
	int fd = memfd_create(file_name, MFD_CLOEXEC);
	
	if (fd < 0)
	{
		PRINT_LOG(FAIL, "Unable to create anonymous file: " BOLD "%s" RESET, file_name);
		
		exit(ERR_FILE);
	}
	
	PRINT_LOG(SUCCESS, "Created anonymous file: " BOLD "%s" RESET, file_name);
	
	if(ftruncate(fd, metadata.buffer_size) != 0)
	{
		PRINT_LOG(FAIL, "Unable to truncate file " BOLD  "%s" RESET " of size " BOLD "%d" RESET, file_name, metadata.frame_size);
		close(fd);
		
		exit(ERR_FILE);
	}

	return fd;
}

// TODO: configure_toplevel: width == 0 and heingth == 0 not necisserily when the window is created 
static void configure_surface(void *data, struct xdg_surface *xdg_surface, uint32_t serial)
{
	START_BENCHMARK(1);
	
	xdg_surface_ack_configure(state.xdg_surface, serial);
	
	switch (state.flag)	
	{
		case CREATE:
			{
				PRINT_LOG(LOG, "Creating frame buffer...");
			
				metadata.stride = metadata.width * 4;
				metadata.frame_size = metadata.stride * metadata.height;

				PRINT_LOG(LOG, "Buffer size = (%d * %d) * %d = " BOLD "%d" RESET " bytes", 
					  metadata.width, 4, metadata.height, metadata.frame_size);
				
				metadata.buffer_size = metadata.frame_size * metadata.len;
				
				PRINT_LOG(LOG, "Total size = %d * %d = " BOLD "%d" RESET " bytes", metadata.frame_size, metadata.len, metadata.buffer_size);
				
				metadata.fd = allocate_shm_file();

				// Casting to pointer to uint8_t for pointer arthimatic
				metadata.buffer = (uint8_t *) mmap(NULL, metadata.buffer_size, PROT_READ | PROT_WRITE, MAP_SHARED, metadata.fd, 0);
				
				if(metadata.buffer == MAP_FAILED)
				{
					PRINT_LOG(FAIL, "Unable to map memory from the annonymous file to " BOLD "%s" RESET, VAR(metadata.buffer));
					close(metadata.fd);

					exit(ERR_MEM);
				}

				PRINT_LOG(SUCCESS, "Mapped memory from the annonymous file to " BOLD "%s" RESET, VAR(metadata.buffer));

				struct wl_shm_pool * pool = wl_shm_create_pool(state.shared_memory, metadata.fd, metadata.buffer_size);
				
				for(uint8_t i = 0; i < metadata.len; i++)
				{
					frame[i].buffer = wl_shm_pool_create_buffer	(pool, 
											 metadata.frame_size * i, 
											 metadata.width, 
											 metadata.height, 
											 metadata.stride, 
											 WL_SHM_FORMAT_ARGB8888);

					frame[i].pixels = (uint32_t *) (metadata.buffer + (metadata.frame_size * i));	

					#ifdef DEBUG
					frame[i].id = i;
					#endif

					wl_buffer_add_listener(frame[i].buffer, &wl_buffer_listener, &frame[i]);
					frame[i].free = true;
				}
				
				PRINT_LOG(LOG, "Created frame buffers: %s and %s", VAR(frame[0]), VAR(frame[1]));
				
				wl_shm_pool_destroy(pool);

				state.flag = DEFAULT;

				goto xdg_surface_configure_end;
			}

		default: break;
	}
	
	wl_callback_destroy(metadata.callback);
	
	xdg_surface_configure_end:

	metadata.callback = wl_surface_frame(state.wl_surface);
	wl_proxy_set_queue((struct wl_proxy *) metadata.callback, state.render_queue);
	
	wl_callback_add_listener(metadata.callback, &wl_callback_listener, data);
	wl_surface_attach(state.wl_surface, frame[0].buffer, 0, 0);
	wl_surface_commit(state.wl_surface);
	
	END_BENCHMARK(1, "%s()", __func__)
}

static void configure_toplevel (void *data, struct xdg_toplevel *xdg_toplevel, int32_t width, int32_t height, struct wl_array *states)
{

	PRINT_LOG(LOG, "Width: %d; Height: %d", width, height);

	if(width == 0 && height == 0) 
	{
		metadata.width = WIDTH;
		metadata.height = HEIGHT;

		state.flag = CREATE;

		PRINT_LOG(LOG, "No height and width specified!");
	}
	else 
	{
		metadata.width = width;
		metadata.height = height;
	}	
}

static void done_callback (void *data, struct wl_callback *wl_callback, uint32_t callback_data)
{
	#ifdef DEBUG 
	static uint32_t time = 0;
	if(time == 0)	time = callback_data;
	
	PRINT_LOG(BENCHMARK, "\u0394callback = %d ms", callback_data - time);
	
	time = callback_data;
	#endif

	wl_callback_destroy(metadata.callback);
	metadata.callback = wl_surface_frame(state.wl_surface);
	wl_proxy_set_queue((struct wl_proxy *) metadata.callback, state.render_queue);
	wl_callback_add_listener(metadata.callback, &wl_callback_listener, data);
	
	struct frame * free_frame;

	for(int i = 0; i < metadata.len; i++)
	{
		if(frame[i].free)
		{	
			PRINT_LOG(LOG, "frame[%d].free = %d", frame[i].id, frame[i].free);
			free_frame = &frame[i];
			goto done_callback_free;
		}
	}
	
	return;	
	
	done_callback_free:
	free_frame->free = false;
	PRINT_LOG(LOG, BOLD "frame[%d]" RESET " chosen for rendering", free_frame->id);

	draw(free_frame->pixels, metadata.height, metadata.width);

	wl_surface_attach(state.wl_surface, free_frame->buffer, 0, 0);
	wl_surface_damage_buffer(state.wl_surface, 0, 0, metadata.width, metadata.height);
	wl_surface_commit(state.wl_surface);
}

static void close_xdg_toplevel(void *data, struct xdg_toplevel *xdg_toplevel)
{
	PRINT_LOG(LOG, "Quitting...");

	state.running = false;
}

static void configure_toplevel_bounds (void *data, struct xdg_toplevel *xdg_toplevel, int32_t width, int32_t height) {}
static void set_wm_capabilities (void *data, struct xdg_toplevel *xdg_toplevel, struct wl_array *capabilities) {}

static void release_buffer(void *data, struct wl_buffer *wl_buffer)
{	
	struct frame * frame_to_be_free = (struct frame *) data;
	frame_to_be_free->free = true;
	
	PRINT_LOG(LOG, BOLD "frame[%d]" RESET " freed", frame_to_be_free->id);
}

int dispatch_display_queue(void * args)
{
	PRINT_LOG(LOG, "Launched " BOLD VAR(dispatch_display_queue()) RESET);
	(void) args;

	int queue_ret_val = 0;
	while(state.running && queue_ret_val != -1)	
		queue_ret_val = wl_display_dispatch_queue(state.display, state.display_queue);
	
	return queue_ret_val;
}

int dispatch_render_queue(void * args)
{
	PRINT_LOG(LOG, "Launched " BOLD VAR(dispatch_render_queue()) RESET);
	(void) args;
		
	int queue_ret_val = 0;
	while(state.running && queue_ret_val != -1)	
		queue_ret_val = wl_display_dispatch_queue(state.display, state.render_queue);
	return queue_ret_val;
}
