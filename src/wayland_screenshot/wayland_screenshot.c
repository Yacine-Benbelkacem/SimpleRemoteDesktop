#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <limits.h>
#include <pixman.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>


#include "buffer.h"
#include "grim.h"
#include "output-layout.h"
#include "render.h"

#include "wayland_screenshot.h"


static void screencopy_frame_handle_buffer(void *data,
										  struct zwlr_screencopy_frame_v1 *frame,
										  uint32_t format,
										  uint32_t width,
										  uint32_t height,
										  uint32_t stride) 
{
	struct grim_output *output = data;

	output->buffer =
		create_buffer(output->state->shm, format, width, height, stride);
	
	if (output->buffer == NULL) {
		fprintf(stderr, "failed to create buffer\n");
		exit(-1);
	}

	zwlr_screencopy_frame_v1_copy(frame, output->buffer->wl_buffer);
}

static void screencopy_frame_handle_flags(void *data,
										  struct zwlr_screencopy_frame_v1 *frame,
										 uint32_t flags)
{
	struct grim_output *output = data;
	output->screencopy_frame_flags = flags;
}

static void screencopy_frame_handle_ready(void *data,
										  struct zwlr_screencopy_frame_v1 *frame,
										  uint32_t tv_sec_hi,
										  uint32_t tv_sec_lo,
										  uint32_t tv_nsec)
{
	struct grim_output *output = data;
	++output->state->n_done;
}

static void screencopy_frame_handle_failed(void *data,
										   struct zwlr_screencopy_frame_v1 *frame)
{
	struct grim_output *output = data;
	fprintf(stderr, "failed to copy output %s\n", output->name);
	exit(-1);
}

static const struct zwlr_screencopy_frame_v1_listener screencopy_frame_listener = {
	.buffer = screencopy_frame_handle_buffer,
	.flags = screencopy_frame_handle_flags,
	.ready = screencopy_frame_handle_ready,
	.failed = screencopy_frame_handle_failed,
};


static void xdg_output_handle_logical_position(void *data,
											   struct zxdg_output_v1 *xdg_output,
											   int32_t x,
											   int32_t y)
{
	struct grim_output *output = data;
	output->logical_geometry.x = x;
	output->logical_geometry.y = y;
}

static void xdg_output_handle_logical_size(void *data,
										   struct zxdg_output_v1 *xdg_output,
										   int32_t width,
										   int32_t height)
{
	struct grim_output *output = data;
	output->logical_geometry.width = width;
	output->logical_geometry.height = height;
}

static void xdg_output_handle_done(void *data,
								   struct zxdg_output_v1 *xdg_output)
{
	struct grim_output *output = data;

	// Guess the output scale from the logical size
	int32_t width = output->geometry.width;
	int32_t height = output->geometry.height;

	apply_output_transform(output->transform, &width, &height);

	output->logical_scale = (double)width / output->logical_geometry.width;
}

static void xdg_output_handle_name(void *data,
								   struct zxdg_output_v1 *xdg_output,
								   const char *name)
{
	struct grim_output *output = data;
	output->name = strdup(name);
}

static void xdg_output_handle_description(void *data,
										  struct zxdg_output_v1 *xdg_output,
										  const char *name)
{
	// No-op
}

static const struct zxdg_output_v1_listener xdg_output_listener = {
	.logical_position = xdg_output_handle_logical_position,
	.logical_size = xdg_output_handle_logical_size,
	.done = xdg_output_handle_done,
	.name = xdg_output_handle_name,
	.description = xdg_output_handle_description,
};


static void output_handle_geometry(void *data,
								   struct wl_output *wl_output,
								   int32_t x,
								   int32_t y,
								   int32_t physical_width,
								   int32_t physical_height,
								   int32_t subpixel,
								   const char *make,
								   const char *model,
								   int32_t transform)
{
	struct grim_output *output = data;

	output->geometry.x = x;
	output->geometry.y = y;
	output->transform = transform;
}

static void output_handle_mode(void *data,
							   struct wl_output *wl_output,
							   uint32_t flags,
							   int32_t width,
							   int32_t height,
							   int32_t refresh)
{
	struct grim_output *output = data;

	if ((flags & WL_OUTPUT_MODE_CURRENT) != 0) {
		output->geometry.width = width;
		output->geometry.height = height;
	}
}

static void output_handle_done(void *data,
							   struct wl_output *wl_output)
{
	// No-op
}

static void output_handle_scale(void *data,
								struct wl_output *wl_output,
								int32_t factor)
{
	struct grim_output *output = data;
	output->scale = factor;
}

static const struct wl_output_listener output_listener = {
	.geometry = output_handle_geometry,
	.mode = output_handle_mode,
	.done = output_handle_done,
	.scale = output_handle_scale,
};


static void handle_global(void *data, 
						  struct wl_registry *registry,
						  uint32_t name,
						  const char *interface,
						  uint32_t version)
{
	struct grim_state *state = data;

	if (strcmp(interface, wl_shm_interface.name) == 0) 
	{
		state->shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
	} 

	else if (strcmp(interface, zxdg_output_manager_v1_interface.name) == 0)
	{
		uint32_t bind_version = (version > 2) ? 2 : version;
		state->xdg_output_manager = wl_registry_bind(registry,
													 name,
													 &zxdg_output_manager_v1_interface,
													 bind_version);
	} 

	else if (strcmp(interface, wl_output_interface.name) == 0)
	{
		struct grim_output *output = calloc(1, sizeof(struct grim_output));
		output->state = state;
		output->scale = 0.5;
		output->wl_output =  wl_registry_bind(registry,
											  name,
											  &wl_output_interface,
											  3);
		wl_output_add_listener(output->wl_output, &output_listener, output);
		wl_list_insert(&state->outputs, &output->link);
	}

	else if (strcmp(interface, zwlr_screencopy_manager_v1_interface.name) == 0)
	{
		state->screencopy_manager = wl_registry_bind(registry,
													 name,
													 &zwlr_screencopy_manager_v1_interface,
													 1);
	}
}

static void handle_global_remove(void *data,
								 struct wl_registry *registry,
								 uint32_t name)
{
	// who cares
}

static const struct wl_registry_listener registry_listener = {
	.global = handle_global,
	.global_remove = handle_global_remove,
};



int wayland_screenshot(pixman_image_t **image)
{	

	struct grim_box *geometry = NULL;
	bool use_greatest_scale = true;
	bool with_cursor = true;
	double scale = 0.5;

	struct grim_state state = {0};

	wl_list_init(&state.outputs);

	state.display = wl_display_connect(NULL);
	if (state.display == NULL) {
		fprintf(stderr, "failed to create display\n");
		return -1;
	}

	state.registry = wl_display_get_registry(state.display);	
	wl_registry_add_listener(state.registry, &registry_listener, &state);
	wl_display_roundtrip(state.display);

	// output interface global
	if (wl_list_empty(&state.outputs)) {
		fprintf(stderr, "no wl_output\n");
		return -1;
	}

	// shm global
	if (state.shm == NULL) {
		fprintf(stderr, "compositor doesn't support wl_shm\n");
		return -1;
	}

	// xdg_output_manager global
	if (state.xdg_output_manager != NULL) 
	{
		
		struct grim_output *output;

		wl_list_for_each(output, &state.outputs, link) 
		{
			output->xdg_output = 
				zxdg_output_manager_v1_get_xdg_output(state.xdg_output_manager,
													  output->wl_output);

			zxdg_output_v1_add_listener(output->xdg_output,
										&xdg_output_listener,
										output);
		}

		wl_display_roundtrip(state.display);
	} 
	else
	{
		fprintf(stderr, "warning: zxdg_output_manager_v1 isn't available, "
			"guessing the output layout\n");

		struct grim_output *output;
		wl_list_for_each(output, &state.outputs, link) {
			guess_output_logical_geometry(output);
		}
	}

	// screencopy global
	if (state.screencopy_manager == NULL) {
		fprintf(stderr, "compositor doesn't support wlr-screencopy-unstable-v1\n");
		return -1;
	}

	size_t n_pending = 0;
	struct grim_output *output;
	wl_list_for_each(output, &state.outputs, link)
	{
		if (use_greatest_scale && output->logical_scale > scale) {
			scale = output->logical_scale;
		}

		// request capture to the screencopy manager on current output
		output->screencopy_frame = 
				zwlr_screencopy_manager_v1_capture_output(state.screencopy_manager,
														  with_cursor,
														  output->wl_output);
		// add a listener to get the request answer (capture event)
		zwlr_screencopy_frame_v1_add_listener(output->screencopy_frame,
											  &screencopy_frame_listener,
											  output);

		++n_pending;
	}

	if (n_pending == 0) {
		fprintf(stderr, "supplied geometry did not intersect with any outputs\n");
		return -1;
	}

	bool done = false;
	while (!done && wl_display_dispatch(state.display) != -1) {
		done = (state.n_done == n_pending);
	}
	if (!done) {
		fprintf(stderr, "failed to screenshoot all outputs\n");
		return -1;
	}

	geometry = calloc(1, sizeof(struct grim_box));
	get_output_layout_extents(&state, geometry);

	*image = (pixman_image_t*) render(&state, geometry, scale);
	if (*image == NULL) {
		return -1;
	}

	struct grim_output *output_tmp;
	wl_list_for_each_safe(output, output_tmp, &state.outputs, link) {
		wl_list_remove(&output->link);
		free(output->name);
		if (output->screencopy_frame != NULL) {
			zwlr_screencopy_frame_v1_destroy(output->screencopy_frame);
		}
		destroy_buffer(output->buffer);
		if (output->xdg_output != NULL) {
			zxdg_output_v1_destroy(output->xdg_output);
		}
		wl_output_release(output->wl_output);
		free(output);
	}
	zwlr_screencopy_manager_v1_destroy(state.screencopy_manager);
	if (state.xdg_output_manager != NULL) {
		zxdg_output_manager_v1_destroy(state.xdg_output_manager);
	}
	wl_shm_destroy(state.shm);
	wl_registry_destroy(state.registry);
	wl_display_disconnect(state.display);
	free(geometry);
	
	return 1;
}
