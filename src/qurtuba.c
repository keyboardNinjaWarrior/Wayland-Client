/* Name: 	Lain			*
 * Email:	ibnul.aftab@proton.me 	*/

// TODO: plan error handling
// TODO: replace exit with proper function that deallocates memory
// TODO: documentation
// XXX:  DON'T TRY TO MAKE ERROR HANDLING IF STATEMENTS INTO A MACRO. EXTREMELY BAD IDEA. PLEASE BEAR WITH IT.
// 	 Strips the abblity to write custom messages.	
// XXX:	 Focus on translating current working application into a library

// TODO: Remove _GNU_SOURCE and replace it with POSIX macro instead for more cross platformness
#define _GNU_SOURCE

#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <threads.h>
#include <wayland-client.h>

#include <sys/mman.h>
#include <linux/memfd.h>

#include "xdg-shell-client-protocol.h"
#include "errors.h"
#include "debug.h"
#include "render.h"

#define STRING(x)	#x
#define NUM_OF_BUFFERS 	2

struct frame 
{	
	struct wl_buffer * buffer;
	uint32_t * pixels;
	bool free;

	#ifdef DEBUG
	uint8_t id;
	#endif
};

struct state 
{
	struct wl_display * display;
	struct wl_registry * registry;
	struct wl_compositor * compositor;
	struct wl_shm * shared_memory;
	struct xdg_wm_base * window_manager_base;
	struct wl_surface * wl_surface;
	struct xdg_surface * xdg_surface;
	struct xdg_toplevel * xdg_toplevel;

	struct wl_event_queue * default_queue;
	struct wl_event_queue * render_queue;
	
	struct wl_callback * callback;
	
	uint8_t * buffer;
	struct frame * frame;

	uint16_t width;
	uint16_t height;
	
	int fd;
	const uint8_t len;
	
	bool running;
};

struct data {
	struct state * state;
	void * arguments;
};

static void ping (void *data, struct xdg_wm_base *xdg_wm_base, uint32_t serial);
static const struct xdg_wm_base_listener window_manager_base_listener = {
	.ping = &ping
};

static void configure_surface(void *data, struct xdg_surface *xdg_surface, uint32_t serial);
static const struct xdg_surface_listener xdg_surface_listener = {
	.configure = &configure_surface  
};

static void configure_toplevel_surface(void *data, struct xdg_toplevel *xdg_toplevel, int32_t width, int32_t height, struct wl_array *states);
static void close_xdg_toplevel(void *data, struct xdg_toplevel *xdg_toplevel);
static void configure_toplevel_bounds (void *data, struct xdg_toplevel *xdg_toplevel, int32_t width, int32_t height);
static void set_wm_capabilities (void *data, struct xdg_toplevel *xdg_toplevel, struct wl_array *capabilities);
static const struct xdg_toplevel_listener xdg_toplevel_listener = {
	.configure = &configure_toplevel_surface,
	.close = &close_xdg_toplevel,
	.configure_bounds = configure_toplevel_bounds,
	.wm_capabilities = set_wm_capabilities
};

static void done_callback (void *data, struct wl_callback *wl_callback, uint32_t callback_data);
static const struct wl_callback_listener wl_callback_listener = {
	.done = &done_callback
};

static void release_buffer(void *data, struct wl_buffer *wl_buffer);
static const struct wl_buffer_listener wl_buffer_listener = {
	.release = &release_buffer
};

static void set_registries(void * data, struct wl_registry * registery, uint32_t name, const char * interface, uint32_t version);
static void handle_removed_registeries (void * data, struct wl_registry * registery, uint32_t name);
static const struct wl_registry_listener wl_registry_listener = {
	.global = &set_registries,
	.global_remove = &handle_removed_registeries
};

// There should be an array that should pe passed
// Maybe it's good to store all of them
static void set_registries(void * data, struct wl_registry * registery, uint32_t name, const char * interface, uint32_t version)
{	
	PRINT_LOG(LOG, "Found: " BOLD "%s" RESET, interface);
	struct state * state = (struct state *) data;

	if (! strcmp(interface, wl_compositor_interface.name))
	{
		state->compositor = wl_registry_bind(registery, name, &wl_compositor_interface, 6 /* wl_compositor_interface.version */);
		
		if(! state->compositor)
		{
			PRINT_LOG(FAIL, "Unable to initialize " BOLD STRING(state->compositor) RESET " from " BOLD STRING(wl_registery_bind()) RESET);
			exit(ERR_COMPOSITOR);
		}
	
		PRINT_LOG(SUCCESS, "Initialized " BOLD STRING(state->compositor) RESET " from " BOLD STRING(wl_registery_bind()) RESET);
		wl_proxy_set_queue((struct wl_proxy *) state->compositor, state->default_queue);
		
		return;
	}

	if (! strcmp(interface, wl_shm_interface.name))
	{

		state->shared_memory = wl_registry_bind(registery, name, &wl_shm_interface, wl_shm_interface.version);
	
		if(! state->shared_memory)
		{
			PRINT_LOG(FAIL, "Unable to initialize " BOLD STRING(state->shared_memory) RESET " from " BOLD STRING(wl_registery_bind()) RESET);
			exit(ERR_SHM);
		}
	
		PRINT_LOG(SUCCESS, "Initialized " BOLD STRING(state->shared_memory) RESET " from " BOLD STRING(wl_registery_bind()) RESET);
		wl_proxy_set_queue((struct wl_proxy *) state->shared_memory, state->default_queue);

		return;
	}

	if (! strcmp(interface, xdg_wm_base_interface.name))
	{

		state->window_manager_base = wl_registry_bind(registery, name, &xdg_wm_base_interface, xdg_wm_base_interface.version);

		if(! state->window_manager_base)
		{ 
			PRINT_LOG(FAIL, "Unable to initialize " BOLD STRING(state->window_manager_base) RESET " from " BOLD STRING(wl_registery_bind()) RESET);
			exit(ERR_XDG_WM_BASE);
		}
	
		PRINT_LOG(SUCCESS, "Initialized " BOLD STRING(state->window_manager_base) RESET " from " BOLD STRING(wl_registery_bind()) RESET);	
		wl_proxy_set_queue((struct wl_proxy *) state->window_manager_base, state->default_queue);
		
		return;
	}
}

static void handle_removed_registeries (void * data, struct wl_registry * registery, uint32_t name) {}

static void ping (void *data, struct xdg_wm_base *xdg_wm_base, uint32_t serial)
{
	PRINT_LOG(LOG, "Application is active!");	
	
	xdg_wm_base_pong(xdg_wm_base, serial);
}

// XXX: Take this out!!!
extern int memfd_create(const char *name, unsigned int flags);

// TODO: Randomize name of file. Maybe use /dev/urandom
inline static int allocate_shm_file(const uint32_t buf_size)
{
	char * const file_name = "frame-buffer";

	// TODO: Replace memfd_create with a POSIX function
	int fd = memfd_create(file_name, MFD_CLOEXEC);
	
	if (fd < 0)
	{
		PRINT_LOG(FAIL, "Unable to create anonymous file: " BOLD "%s" RESET, file_name);
		exit(ERR_FILE);
	}
	
	PRINT_LOG(SUCCESS, "Created anonymous file: " BOLD "%s" RESET, file_name);
	
	if(ftruncate(fd, buf_size) != 0)
	{
		PRINT_LOG(FAIL, "Unable to truncate file " BOLD  "%s" RESET " to size " BOLD "%d" RESET, file_name, buf_size);
		close(fd);
		
		exit(ERR_FILE);
	}

	return fd;
}


// TODO: configure_toplevel: width == 0 and heingth == 0 not necisserily when the window is created 
// NOTE: metadata.len was number of frame buffers
static void configure_surface(void *data, struct xdg_surface *xdg_surface, uint32_t serial)
{
	START_BENCHMARK(1);

	struct state * state = (struct state *) data;

	xdg_surface_ack_configure(state->xdg_surface, serial);
	
	if(! state->buffer)
	{
		PRINT_LOG(LOG, "Creating frame buffer...");

		const uint32_t stride = state->width * 4;
		const uint32_t frame_size = stride * state->height;

		PRINT_LOG(LOG, "Each frame is of size " BOLD "%d" RESET " bytes", frame_size);
		
		const uint32_t buf_size = frame_size * NUM_OF_BUFFERS;
		
		PRINT_LOG(LOG, "Total buffer size = " BOLD "%d" RESET " bytes", buf_size);
		
		state->fd = allocate_shm_file(buf_size);

		// Casting to pointer to uint8_t for pointer arthimatic
		state->buffer = (uint8_t *) mmap(NULL, buf_size, PROT_READ | PROT_WRITE, MAP_SHARED, state->fd, 0);
		if(state->buffer == MAP_FAILED)
		{
			PRINT_LOG(FAIL, "Unable to map memory from the annonymous file to " BOLD STRING(state->buffer) RESET);
			close(state->fd);

			exit(ERR_MEM);
		}

		PRINT_LOG(SUCCESS, "Mapped memory from the annonymous file to " BOLD "%s" RESET, STRING(metadata.buffer));

		struct wl_shm_pool * pool = wl_shm_create_pool(state->shared_memory, state->fd, buf_size);
		if (! (state->frame = (struct frame *) calloc(NUM_OF_BUFFERS, sizeof(struct frame))))
		{
			PRINT_LOG(FAIL, "Unable to allocate memory for " STRING(state->frame));
			exit(ERR_MEM);
		}

		for(uint8_t i = 0; i < NUM_OF_BUFFERS; i++)
		{
			state->frame[i].buffer = wl_shm_pool_create_buffer	(pool, 
										 frame_size * i, 
										 state->width, 
										 state->height, 
										 stride, 
										 WL_SHM_FORMAT_ARGB8888);

			state->frame[i].pixels = (uint32_t *) (state->buffer + (frame_size * i));
			wl_buffer_add_listener(state->frame[i].buffer, &wl_buffer_listener, &(state->frame[i]));
			state->frame[i].free = true;
		}
		
		PRINT_LOG(LOG, "Created frame buffers...");
		
		wl_shm_pool_destroy(pool);
		goto xdg_surface_configure_end;

	}
	
	wl_callback_destroy(state->callback);
	
	xdg_surface_configure_end:
	state->callback = wl_surface_frame(state->wl_surface);
	wl_proxy_set_queue((struct wl_proxy *) state->callback, state->render_queue);
	wl_callback_add_listener(state->callback, &wl_callback_listener, data);
	for(int i = 0; i < NUM_OF_BUFFERS; i++)
	{
		if(state->frame[i].free)
		{
			wl_surface_attach(state->wl_surface, state->frame[0].buffer, 0, 0);
			break;
		}
	}
	wl_surface_commit(state->wl_surface);
	
	END_BENCHMARK(1, "Event: " BOLD "%s()" RESET, __func__)
}

static void configure_toplevel_surface(void *data, struct xdg_toplevel *xdg_toplevel, int32_t width, int32_t height, struct wl_array *states)
{
	PRINT_LOG(LOG, "Event " BOLD "%s" RESET " dispatched..." "\n" "Width = %d; Height = %d", __func__, width, height);

	struct state * state = (struct state *) ((struct data *) data)->state;
	struct 
	{
		uint16_t width;
		uint16_t height;
	} * dimensions = ((struct data *) data)->arguments;
	
	if(width == 0 && height == 0) 
	{
		PRINT_LOG(LOG, "No height and width specified!" "\n" "Setting default width (%d) and height (%d)...", dimensions->width, dimensions->height);
		
		state->width = dimensions->width;
		state->height = dimensions->height;
	}
	else 
	{
		state->width = width;
		state->height = height;
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

	struct state * state = (struct state *) data;
	
	wl_callback_destroy(state->callback);
	state->callback = wl_surface_frame(state->wl_surface);
	wl_proxy_set_queue((struct wl_proxy *) state->callback, state->render_queue);
	wl_callback_add_listener(state->callback, &wl_callback_listener, data);
	
	struct frame * free_frame;

	for(int i = 0; i < NUM_OF_BUFFERS; i++)
	{
		PRINT_LOG(LOG, "frame[%d] is " BOLD "%s" RESET, i, state->frame[i].free ? "free" : "not free");
		
		if(state->frame[i].free)
		{	
			free_frame = &(state->frame[i]);
			PRINT_LOG(LOG, BOLD "frame[%d]" RESET " chosen for rendering", i);
			
			goto done_callback_free;
		}
	}
	
	return;	
	
	done_callback_free:
	free_frame->free = false;

	draw(free_frame->pixels, state->height, state->width);

	wl_surface_attach(state->wl_surface, free_frame->buffer, 0, 0);
	wl_surface_damage_buffer(state->wl_surface, 0, 0, state->width, state->height);
	wl_surface_commit(state->wl_surface);
}

static void close_xdg_toplevel(void *data, struct xdg_toplevel *xdg_toplevel)
{
	PRINT_LOG(LOG, "Quitting:...");
	
	struct state * state = (struct state *) data;
	state->running = false;
	PRINT_LOG(LOG, "%s", state->running ? "running true" : "running false");
}

static void configure_toplevel_bounds (void *data, struct xdg_toplevel *xdg_toplevel, int32_t width, int32_t height) {}
static void set_wm_capabilities (void *data, struct xdg_toplevel *xdg_toplevel, struct wl_array *capabilities) {}

static void release_buffer(void *data, struct wl_buffer *wl_buffer)
{	
	struct frame * frame_to_be_free = (struct frame *) data;
	frame_to_be_free->free = true;
	
	PRINT_LOG(LOG, BOLD "frame[%d]" RESET " freed", frame_to_be_free->id);
}

static int dispatch_display_queue(void * args)
{
	PRINT_LOG(LOG, "Launched " BOLD STRING(dispatch_display_queue()) RESET);
      	
	struct state * state = (struct state *) args;

	int queue_ret_val = 0;
	while(state->running && queue_ret_val != -1)	
		queue_ret_val = wl_display_dispatch_queue(state->display, state->default_queue);
	
	return queue_ret_val;
	return 0;
}

static int dispatch_render_queue(void * args)
{
	PRINT_LOG(LOG, "Launched " BOLD STRING(dispatch_render_queue()) RESET);
	struct state * state = (struct state *) args;
		
	int queue_ret_val = 0;
	while(state->running && queue_ret_val != -1)
		queue_ret_val = wl_display_dispatch_queue(state->display, state->render_queue);
	return queue_ret_val;
	return 0;
}


struct state * qurtuba_create_window(char * title, uint16_t width, uint16_t height)
{
	struct state * state = (struct state *) malloc(sizeof(struct state));
	memset((void *) state, 0, sizeof(struct state));	

	if(! (state->display = wl_display_connect(NULL)))
	{
		PRINT_LOG(FAIL, "Unable to initialize " BOLD STRING(state->display) RESET " from " BOLD STRING(wl_display_connect()) RESET);
		exit(ERR_DISPLAY);
	}
	
	PRINT_LOG(SUCCESS, "Initialize " BOLD "state->display" RESET " from " BOLD STRING(wl_display_connect()) RESET);

	if(! (state->default_queue = wl_display_create_queue(state->display)))
	{
		PRINT_LOG(FAIL, "Unable to initialize " BOLD STRING(state->default_queue) RESET " from " BOLD STRING(wl_display_create_queue()) RESET);
		exit(ERR_DISPLAY);
	}
	
	PRINT_LOG(SUCCESS, "Initialized " BOLD STRING(state->default_queue) RESET " from " BOLD STRING(wl_display_create_queue()) RESET);

	state->registry = wl_display_get_registry(state->display);
	wl_proxy_set_queue((struct wl_proxy *) state->registry, state->default_queue);
	wl_registry_add_listener(state->registry, &wl_registry_listener, (void *) state);
	
	START_BENCHMARK(1);
	
	// Currently, it only binds compositor, shared memory and a window manager
	// TODO: Support for binding custom registries

	wl_display_roundtrip_queue(state->display, state->default_queue);	

	END_BENCHMARK(1, "Function " BOLD STRING(wl_display_roundtrip_queue(state->display, state->default_queue)) RESET);
	
	xdg_wm_base_add_listener(state->window_manager_base, &window_manager_base_listener, NULL);

	if(! (state->wl_surface = wl_compositor_create_surface(state->compositor)))
	{
		PRINT_LOG(FAIL, "Unable to initialize " BOLD STRING(state->wl_surface) RESET " from " BOLD STRING(wl_compositor_create_surface()) RESET);
		exit(ERR_WL_SURFACE);
	}

	PRINT_LOG(SUCCESS, "Initialized " BOLD STRING(state->wl_surface) RESET " from " BOLD STRING(wl_display_connect()) RESET);
	
	if(! (state->xdg_surface = xdg_wm_base_get_xdg_surface(state->window_manager_base, state->wl_surface)))
	{
		PRINT_LOG(FAIL, "Unable to initialize " BOLD STRING(state->wl_surface) RESET " from " BOLD STRING(xdg_wm_base_get_xdg_surface()) RESET);
		exit(ERR_XDG_SURFACE);
	}

	PRINT_LOG(SUCCESS, "Initialized " BOLD STRING(state->xdg_surface) RESET " from " BOLD STRING(xdg_wm_base_get_xdg_surface()) RESET);
	
	xdg_surface_add_listener(state->xdg_surface, &xdg_surface_listener, state);
		
	if(! (state->xdg_toplevel = xdg_surface_get_toplevel(state->xdg_surface)))
	{
		PRINT_LOG(FAIL, "Unable to initialize " BOLD STRING(state->xdg_toplevel) RESET " from " BOLD STRING(xdg_surface_get_toplevel()) RESET);
		exit(ERR_XDG_SURFACE);
	}
	
	PRINT_LOG(SUCCESS, "Initialized " BOLD STRING(state->xdg_toplevel) RESET " from " BOLD STRING(xdg_surface_get_toplevel()) RESET);

	// TODO: Try converting them into annonymous objects	
	struct
	{
		uint16_t width;
		uint16_t height;
	} dimensions = {
		.width = width,
		.height = height
	};

	struct data data = {
		.state = state,
		.arguments = (void *) &dimensions 
	};

	xdg_toplevel_add_listener(state->xdg_toplevel, &xdg_toplevel_listener, (void *) &data);
	xdg_toplevel_set_title(state->xdg_toplevel, title);
	
	if(! (state->render_queue = wl_display_create_queue(state->display)))
	{
		PRINT_LOG(FAIL, "Unable to initialize " BOLD STRING(state->render_queue) RESET " from " BOLD STRING(wl_display_create_queue()) RESET);
		exit(ERR_DISPLAY);
	}
	
	PRINT_LOG(SUCCESS, "Initialized " BOLD STRING(state->render_queue) RESET " from " BOLD STRING(wl_display_create_queue()) RESET);

	wl_surface_commit(state->wl_surface);

	state->running = true;

	thrd_t display_queue_thrd_id;
	thrd_create(&display_queue_thrd_id, dispatch_display_queue, (void *) state);
	
	int display_queue_thrd_ret_val;

	thrd_t render_queue_thrd_id;
	thrd_create(&render_queue_thrd_id, dispatch_render_queue, (void *) state);
	int render_queue_thrd_ret_val; 
	
	thrd_join(render_queue_thrd_id, &render_queue_thrd_ret_val);
	thrd_join(display_queue_thrd_id, &display_queue_thrd_ret_val);
	
	return state;
}
