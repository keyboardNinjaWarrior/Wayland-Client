/* Name: 	Lain			*
 * Email:	ibnul.aftab@proton.me 	*/

// TODO: plan error handling
// TODO: replace exit with proper function that deallocates memory
// TODO: documentation
// XXX:  DON'T TRY TO MAKE ERROR HANDLING IF STATEMENTS INTO A MACRO. EXTREMELY BAD IDEA. PLEASE BEAR WITH IT.
// 	 Strips the abblity to write custom messages.	
// XXX:	 Focus on translating current working application into a library
// BUG:	 The window is unable to close itself _sometimes_.

// TODO: Remove _GNU_SOURCE and replace it with POSIX macro instead for more cross platformness
#define _GNU_SOURCE

// Standard Libraries
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <inttypes.h>

// Unix Libraries
#include <threads.h>
#include <wayland-client.h>
#include <sys/mman.h>
#include <linux/memfd.h>

// Local Libraries
#include "xdg-shell-client-protocol.h"
#include "errors.h"
#include "debug.h"

#define STRING(x)	#x
#define NUM_OF_BUFFERS 	2
#define STATE(x) 	((struct state *) (x))
#define MIN(x,y)	(((x) < (y)) ? (x) : (y))

/* Global Singelton Objects
 * These are initilized at the start of window creation once only. These include:
 * 	1. display
 * 	2. registry
 *	3. compositor
 *	4. shared memory
 * 	5. singleton_queue
 * 	6. global_object (list)
 */

struct wl_display * display;
struct wl_registry * registry;
struct wl_compositor * compositor;
struct wl_shm * shared_memory;

struct wl_event_queue * singleton_queue;

static struct global_object
{
	// 64 bit
	struct wl_registry * registry;
	struct global_object * next;
	const char * interface;
	
	// 32 bit
	uint32_t name;
	uint32_t version;

} * global_object = nullptr;

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
	struct xdg_wm_base * window_manager_base;
	struct wl_surface * wl_surface;
	struct xdg_surface * xdg_surface;
	struct xdg_toplevel * xdg_toplevel;

	struct wl_event_queue * default_queue;
	struct wl_event_queue * render_queue;
	
	struct wl_callback * callback;
	void (* draw) (uint32_t *, uint16_t, uint16_t);
	
	uint8_t * buffer;
	struct frame * frame;

	uint16_t width;
	uint16_t height;
	
	uint16_t width_to_set;
	uint16_t height_to_set;

	int fd;
	const uint8_t len;
	
	bool running;
};

static void ping (void * data, struct xdg_wm_base * xdg_wm_base, uint32_t serial);
static const struct xdg_wm_base_listener window_manager_base_listener = {
	.ping = &ping
};

static void configure_surface(void * data, struct xdg_surface * xdg_surface, uint32_t serial);
static const struct xdg_surface_listener xdg_surface_listener = {
	.configure = &configure_surface  
};

static void configure_toplevel_surface(void * data, struct xdg_toplevel * xdg_toplevel, int32_t width, int32_t height, struct wl_array * states);
static void close_xdg_toplevel(void * data, struct xdg_toplevel * xdg_toplevel);
static void configure_toplevel_bounds (void * data, struct xdg_toplevel * xdg_toplevel, int32_t width, int32_t height);
static void set_wm_capabilities (void * data, struct xdg_toplevel * xdg_toplevel, struct wl_array * capabilities);
static const struct xdg_toplevel_listener xdg_toplevel_listener = {
	.configure = &configure_toplevel_surface,
	.close = &close_xdg_toplevel,
	.configure_bounds = &configure_toplevel_bounds,
	.wm_capabilities = &set_wm_capabilities
};

static void done_callback (void * data, struct wl_callback * wl_callback, uint32_t callback_data);
static const struct wl_callback_listener wl_callback_listener = {
	.done = &done_callback
};

static void release_buffer(void * data, struct wl_buffer * wl_buffer);
static const struct wl_buffer_listener wl_buffer_listener = {
	.release = &release_buffer
};

static void create_global_object_list (void * data, struct wl_registry * registry, uint32_t name, const char * interface, uint32_t version);
static void handle_removed_registries (void * data, struct wl_registry * registry, uint32_t name);
static const struct wl_registry_listener wl_registry_listener = {
	.global = &create_global_object_list,
	.global_remove = &handle_removed_registries
};

/* Creates  an  array of  global  objects available  globally.  The  purpose  is  to  prevent  multiple
 * get_wl_registries() requiest to be fired.
 * 	> It should  be noted that the server  side  resources  consumed in response to a  get_registry
 *	  request can only be released when  the client disconnects,  not when the client side proxy is 
 *	  destroyed. Therefore, clients should invoke get_registry as infrequently as possible to avoid
 *	  wasting memory.
 */
static void create_global_object_list (void * data, struct wl_registry * registry, uint32_t name, const char * interface, uint32_t version)
{
	PRINT_LOG(LOG, "Found:" "\n\t" 
		       "registry: " BOLD "%p" RESET "\n\t" 
		       "name: " BOLD "%" PRIu32 RESET "\n\t" 
		       "interface: " BOLD "%s" RESET "\n\t" 
		       "version: " BOLD "%" PRIu32 RESET, 
		       registry, name, interface, version);

	static struct global_object ** node = &global_object;
	
	if (! strcmp(interface, wl_compositor_interface.name))
	{
		if(! (compositor = wl_registry_bind(registry, name, &wl_compositor_interface, MIN(wl_compositor_interface.version, version))))
		{
			PRINT_LOG(FAIL, "Unable to initialize " BOLD STRING(compositor) RESET " from " BOLD STRING(wl_registry_bind()) RESET);
			exit(ERR_COMPOSITOR);
		}
	
		PRINT_LOG(SUCCESS, "Initialized " BOLD STRING(compositor) RESET " from " BOLD STRING(wl_registry_bind()) RESET);

		// TODO: determine the queue
		wl_proxy_set_queue((struct wl_proxy *) compositor, singleton_queue);
		
		return;
	}
	
	if (! strcmp(interface, wl_shm_interface.name))
	{	
		if(! (shared_memory = wl_registry_bind(registry, name, &wl_shm_interface, MIN(wl_shm_interface.version, version))))
		{
			PRINT_LOG(FAIL, "Unable to initialize " BOLD STRING(shared_memory) RESET " from " BOLD STRING(wl_registry_bind()) RESET);
			exit(ERR_SHM);
		}
	
		PRINT_LOG(SUCCESS, "Initialized " BOLD STRING(shared_memory) RESET " from " BOLD STRING(wl_registry_bind()) RESET);
		
		wl_proxy_set_queue((struct wl_proxy *) shared_memory, singleton_queue);

		return;
	}

	// node is a pointer to pointer  to a struct global_object. So, 
	// drefrencing  it  would   produce  a  pointer  to the  struct 
	// global_object

	*node = (struct global_object *) malloc(sizeof(struct global_object));
	
#define GLOBAL_OBJECT(pointer) (*(pointer))
	
	GLOBAL_OBJECT(node)->registry = registry;
	GLOBAL_OBJECT(node)->name = name;
	GLOBAL_OBJECT(node)->interface = interface;
	GLOBAL_OBJECT(node)->version = version;
	GLOBAL_OBJECT(node)->next = nullptr;

	node = &(GLOBAL_OBJECT(node)->next);

#undef 	GLOBAL_OBJECT
	
	return;
}

// TODO: Pass global_object instead of these parameters
static inline void bind_global_objects(struct state * state, struct global_object * global_object)
{	
	PRINT_LOG(LOG, "Global Object:" "\n\t" 
		       "registry: " BOLD "%p" RESET "\n\t" 
		       "name: " BOLD "%" PRIu32 RESET "\n\t" 
		       "interface: " BOLD "%s" RESET "\n\t" 
		       "version: " BOLD "%" PRIu32 RESET,
		       registry, global_object->name, global_object->interface, global_object->version);

	if (! strcmp(global_object->interface, xdg_wm_base_interface.name))
	{
		if(! (state->window_manager_base = wl_registry_bind(registry, global_object->name, &xdg_wm_base_interface, xdg_wm_base_interface.version)))
		{ 
			PRINT_LOG(FAIL, "Unable to initialize " BOLD STRING(state->window_manager_base) RESET " from " BOLD STRING(wl_registry_bind()) RESET);
			exit(ERR_XDG_WM_BASE);
		}
	
		PRINT_LOG(SUCCESS, "Initialized " BOLD STRING(state->window_manager_base) RESET " from " BOLD STRING(wl_registry_bind()) RESET);	

		wl_proxy_set_queue((struct wl_proxy *) state->window_manager_base, state->default_queue);
		
		return;
	}	
}

static void handle_removed_registries (void * data, struct wl_registry * registry, uint32_t name) {}

static void ping (void * data, struct xdg_wm_base *xdg_wm_base, uint32_t serial)
{
	PRINT_LOG(LOG, "Application is active!");	
	
	(void) data;	
	
	xdg_wm_base_pong(xdg_wm_base, serial);
}

// NOTE: Take this out!!!
extern int memfd_create(const char * name, unsigned int flags);

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


// TODO: configure_toplevel: width == 0 and height == 0 not necisserily when the window is created 
// NOTE: metadata.len was number of frame buffers
static void configure_surface(void *data, struct xdg_surface *xdg_surface, uint32_t serial)
{
	START_BENCHMARK(1);

	xdg_surface_ack_configure(STATE(data)->xdg_surface, serial);
	
	if(! STATE(data)->buffer)
	{
		PRINT_LOG(LOG, "Creating frame buffer...");

		const uint32_t stride = STATE(data)->width * 4;
		const uint32_t frame_size = stride * STATE(data)->height;

		PRINT_LOG(LOG, "Each frame is of size " BOLD "%d" RESET " bytes", frame_size);
		
		const uint32_t buf_size = frame_size * NUM_OF_BUFFERS;
		
		PRINT_LOG(LOG, "Total buffer size = " BOLD "%d" RESET " bytes", buf_size);
		
		STATE(data)->fd = allocate_shm_file(buf_size);

		// Casting to pointer to uint8_t for pointer arthimatic
		STATE(data)->buffer = (uint8_t *) mmap(nullptr, buf_size, PROT_READ | PROT_WRITE, MAP_SHARED, STATE(data)->fd, 0);
		if(STATE(data)->buffer == MAP_FAILED)
		{
			PRINT_LOG(FAIL, "Unable to map memory from the annonymous file to " BOLD STRING(STATE(data)->buffer) RESET);
			close(STATE(data)->fd);

			exit(ERR_MEM);
		}

		PRINT_LOG(SUCCESS, "Mapped memory from the annonymous file to " BOLD "%s" RESET, STRING(state->buffer));

		struct wl_shm_pool * pool = wl_shm_create_pool(shared_memory, STATE(data)->fd, buf_size);

		if (! (STATE(data)->frame = (struct frame *) calloc(NUM_OF_BUFFERS, sizeof(struct frame))))
		{
			PRINT_LOG(FAIL, "Unable to allocate memory for " STRING(STATE(data)->frame));
			exit(ERR_MEM);
		}

		for(uint8_t i = 0; i < NUM_OF_BUFFERS; i++)
		{
			STATE(data)->frame[i].buffer = wl_shm_pool_create_buffer	(pool, 
											 frame_size * i, 
											 STATE(data)->width, 
											 STATE(data)->height, 
											 stride, 
											 WL_SHM_FORMAT_ARGB8888);

			STATE(data)->frame[i].pixels = (uint32_t *) (STATE(data)->buffer + (frame_size * i));
			wl_buffer_add_listener(STATE(data)->frame[i].buffer, &wl_buffer_listener, &(STATE(data)->frame[i]));
			STATE(data)->frame[i].free = true;
		}
		
		PRINT_LOG(LOG, "Created frame buffers...");
		
		wl_shm_pool_destroy(pool);
		goto xdg_surface_configure_end;

	}
	
	wl_callback_destroy(STATE(data)->callback);
	
	xdg_surface_configure_end:
	STATE(data)->callback = wl_surface_frame(STATE(data)->wl_surface);
	wl_proxy_set_queue((struct wl_proxy *) STATE(data)->callback, STATE(data)->render_queue);
	wl_callback_add_listener(STATE(data)->callback, &wl_callback_listener, data);
	for(int i = 0; i < NUM_OF_BUFFERS; i++)
	{
		if(STATE(data)->frame[i].free)
		{
			wl_surface_attach(STATE(data)->wl_surface, STATE(data)->frame[0].buffer, 0, 0);
			break;
		}
	}
	wl_surface_commit(STATE(data)->wl_surface);
	
	END_BENCHMARK(1, "Event: " BOLD "%s()" RESET, __func__)
}

static void configure_toplevel_surface(void *data, struct xdg_toplevel *xdg_toplevel, int32_t width, int32_t height, struct wl_array *states)
{
	PRINT_LOG(LOG, "Event " BOLD "%s" RESET " dispatched..." "\n" LOG " " "Width = %d; Height = %d", __func__, width, height);
	
	if(width == 0 && height == 0) 
	{
		PRINT_LOG(LOG, "No height and width specified!" "\n" "Setting default width (%" PRIu16 ") and height (%" PRIu16 ")...", 
				STATE(data)->width_to_set, STATE(data)->height_to_set);
		
		STATE(data)->width = STATE(data)->width_to_set;
		STATE(data)->height = STATE(data)->height_to_set;

	}
	else 
	{
		STATE(data)->width = width;
		STATE(data)->height = height;
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
	
	wl_callback_destroy(STATE(data)->callback);
	STATE(data)->callback = wl_surface_frame(STATE(data)->wl_surface);
	wl_proxy_set_queue((struct wl_proxy *) STATE(data)->callback, STATE(data)->render_queue);
	wl_callback_add_listener(STATE(data)->callback, &wl_callback_listener, data);
	
	struct frame * free_frame;

	for(int i = 0; i < NUM_OF_BUFFERS; i++)
	{
		PRINT_LOG(LOG, "frame[%d] is " BOLD "%s" RESET, i, STATE(data)->frame[i].free ? "free" : "not free");
		
		if(STATE(data)->frame[i].free)
		{	
			free_frame = &(STATE(data)->frame[i]);
			PRINT_LOG(LOG, BOLD "frame[%d]" RESET " chosen for rendering", i);
			
			goto done_callback_free;
		}
	}
	
	return;	
	
	done_callback_free:
	free_frame->free = false;

	STATE(data)->draw(free_frame->pixels, STATE(data)->height, STATE(data)->width);

	wl_surface_attach(STATE(data)->wl_surface, free_frame->buffer, 0, 0);
	wl_surface_damage_buffer(STATE(data)->wl_surface, 0, 0, STATE(data)->width, STATE(data)->height);
	wl_surface_commit(STATE(data)->wl_surface);
}

static void close_xdg_toplevel(void * data, struct xdg_toplevel * xdg_toplevel)
{
	PRINT_LOG(LOG, "Quitting...");
	
	STATE(data)->running = false;
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

	int queue_ret_val = 0;
	while(STATE(args)->running && queue_ret_val != -1)	
		queue_ret_val = wl_display_dispatch_queue(display, STATE(args)->default_queue);
	
	return queue_ret_val;
}

static int dispatch_render_queue(void * args)
{
	PRINT_LOG(LOG, "Launched " BOLD STRING(dispatch_render_queue()) RESET);
		
	int queue_ret_val = 0;
	while(STATE(args)->running && queue_ret_val != -1)
		queue_ret_val = wl_display_dispatch_queue(display, STATE(args)->render_queue);

	return queue_ret_val;
}

static int dispatch_singleton_queue(void * args)
{
	PRINT_LOG(LOG, "Launched " BOLD STRING(dispatch_singleton_queue()) RESET);
		
	int queue_ret_val = 0;
	while(STATE(args)->running && queue_ret_val != -1)
		queue_ret_val = wl_display_dispatch_queue(display, singleton_queue); 

	return queue_ret_val;
}

void qurtuba_init(void)
{
	// Initializing IPC with the compositor
	if(! (display = wl_display_connect(nullptr)))
	{
		PRINT_LOG(FAIL, "Unable to initialize " BOLD STRING(display) RESET " from " BOLD STRING(wl_display_connect()) RESET);
		exit(ERR_DISPLAY);
	}
	
	PRINT_LOG(SUCCESS, "Initialize " BOLD STRING(display) RESET " from " BOLD STRING(wl_display_connect()) RESET);

	if(! (singleton_queue = wl_display_create_queue(display)))
	{
		PRINT_LOG(FAIL, "Unable to initialize " BOLD STRING(singleton_queue) RESET " from " BOLD STRING(wl_display_create_queue()) RESET);
		exit(ERR_DISPLAY);
	}
	
	PRINT_LOG(SUCCESS, "Initialized " BOLD STRING(singleton_queue) RESET " from " BOLD STRING(wl_display_create_queue()) RESET);
	
	registry = wl_display_get_registry(display);
	wl_proxy_set_queue((struct wl_proxy *) registry, singleton_queue);			// wl_registry will use singleton_queue to execute events
	wl_registry_add_listener(registry, &wl_registry_listener, nullptr);
	
	START_BENCHMARK(1);
	
	// NOTE: Currently, it only binds compositor, shared memory and a window manager
	// TODO: Support for binding custom registries

	wl_display_roundtrip_queue(display, singleton_queue);	

	END_BENCHMARK(1, "Function " BOLD STRING(wl_display_roundtrip_queue(display, singleton_queue)) RESET);
}

struct state * qurtuba_create_window(char * title, uint16_t width, uint16_t height, void (* draw) (uint32_t *, uint16_t, uint16_t))
{
	START_BENCHMARK(2);

	struct state * state = (struct state *) malloc(sizeof(struct state));
	memset((void *) state, 0, sizeof(struct state));	

	END_BENCHMARK(2, "heap allocation of " BOLD STRING(struct state) RESET);

	if(! (state->default_queue = wl_display_create_queue(display)))
	{
		PRINT_LOG(FAIL, "Unable to initialize " BOLD STRING(state->default_queue) RESET " from " BOLD STRING(wl_display_create_queue()) RESET);
		exit(ERR_DISPLAY);
	}
	
	PRINT_LOG(SUCCESS, "Initialized " BOLD STRING(state->default_queue) RESET " from " BOLD STRING(wl_display_create_queue()) RESET);

	for(struct global_object * i = global_object; i; i = i->next)
	{
		bind_global_objects(state, i); 
	}
		
	xdg_wm_base_add_listener(state->window_manager_base, &window_manager_base_listener, nullptr);

	if(! (state->wl_surface = wl_compositor_create_surface(compositor)))
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
	
	state->draw = draw;

	xdg_surface_add_listener(state->xdg_surface, &xdg_surface_listener, (void *) state);
		
	if(! (state->xdg_toplevel = xdg_surface_get_toplevel(state->xdg_surface)))
	{
		PRINT_LOG(FAIL, "Unable to initialize " BOLD STRING(state->xdg_toplevel) RESET " from " BOLD STRING(xdg_surface_get_toplevel()) RESET);
		exit(ERR_XDG_SURFACE);
	}
	
	PRINT_LOG(SUCCESS, "Initialized " BOLD STRING(state->xdg_toplevel) RESET " from " BOLD STRING(xdg_surface_get_toplevel()) RESET);

	state->width_to_set = width;
	state->height_to_set = height;

	// TODO: Try converting them into annonymous objects
	// TODO: Heap allocation
	xdg_toplevel_add_listener(state->xdg_toplevel, &xdg_toplevel_listener, (void *) state);
	xdg_toplevel_set_title(state->xdg_toplevel, title);
	
	if(! (state->render_queue = wl_display_create_queue(display)))
	{
		PRINT_LOG(FAIL, "Unable to initialize " BOLD STRING(state->render_queue) RESET " from " BOLD STRING(wl_display_create_queue()) RESET);
		exit(ERR_DISPLAY);
	}
	
	PRINT_LOG(SUCCESS, "Initialized " BOLD STRING(state->render_queue) RESET " from " BOLD STRING(wl_display_create_queue()) RESET);

	wl_surface_commit(state->wl_surface);
	
	return state;
}

void qurtuba_launch_window(struct state * state)
{
	state->running = true;

	thrd_t registry_queue_thrd_id;
	thrd_create(&registry_queue_thrd_id, dispatch_singleton_queue, (void *) state);
	int registry_queue_thrd_ret_val = 0;
	
	thrd_t display_queue_thrd_id = 0;
	thrd_create(&display_queue_thrd_id, dispatch_display_queue, (void *) state);
	int display_queue_thrd_ret_val = 0;

	thrd_t render_queue_thrd_id = 0;
	thrd_create(&render_queue_thrd_id, dispatch_render_queue, (void *) state);
	int render_queue_thrd_ret_val = 0;
	
	// TODO: Remove this and put it maybe in qurtuba_close_window()
	thrd_join(registry_queue_thrd_id, &registry_queue_thrd_ret_val);
	thrd_join(render_queue_thrd_id, &render_queue_thrd_ret_val);
	thrd_join(display_queue_thrd_id, &display_queue_thrd_ret_val);
}

void qurtuba_close_window(struct state * state)
{
	for(int i = 0; i < NUM_OF_BUFFERS; i++)	wl_buffer_destroy(state->frame[i].buffer);

	wl_callback_destroy(state->callback);
	xdg_toplevel_destroy(state->xdg_toplevel);
	xdg_surface_destroy(state->xdg_surface);
	wl_surface_destroy(state->wl_surface);
	xdg_wm_base_destroy(state->window_manager_base);
	
	const uint32_t stride = state->width * 4;
	const uint32_t frame_size = stride * state->height;
	const uint32_t buf_size = frame_size * NUM_OF_BUFFERS;

	if(munmap(state->buffer, buf_size) < 0)
	{
		PRINT_LOG(FAIL, "Unable to free memory: " BOLD STRING(state->buffer) RESET " having size = " BOLD "%d" RESET, buf_size);
		exit(ERR_MEM);
	}
		
	close(state->fd);
	free(state->frame);
	free(state);
}

// 1. Releases memory allocated for the list global_object
// 2. Destroys:
// 	- registry_queue
// 	- registry
// 	- compositor
// 	- shared memory
// 3. Disconnects IPC connection with compositor
void qurtuba_exit(void)
{
	for(struct global_object * i = global_object; i;)
	{
		struct global_object * next = i->next;
		free(i);
		i = next;
	}

	wl_shm_destroy(shared_memory);
	wl_compositor_destroy(compositor);
	wl_registry_destroy(registry);
	
	wl_event_queue_destroy(singleton_queue);
	
	wl_display_disconnect(display);
}
