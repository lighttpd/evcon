
#include <evcon-glib.h>

#include <evcon-allocator.h>

#include <evcon-config-private.h>

#define UNUSED(x) ((void)(x))

/* GLib slice wrapper */

static void* evcon_glib_alloc_cb(size_t size, void* user_data) {
	UNUSED(user_data);
	return g_slice_alloc(size);
}

static void evcon_glib_free_cb(void *ptr, size_t size, void *user_data) {
	UNUSED(user_data);
	g_slice_free1(size, ptr);
}

evcon_allocator* evcon_glib_allocator(void) {
	static char static_allocator_buf[EVCON_ALLOCATOR_RECOMMENDED_SIZE];
	static volatile evcon_allocator* allocator = NULL;
	static volatile int lock = 0;

	if (2 == g_atomic_int_get(&lock)) return (evcon_allocator*) allocator;

	while (!g_atomic_int_compare_and_exchange(&lock, 0, 1)) {
		if (2 == g_atomic_int_get(&lock)) return (evcon_allocator*) allocator;
	}

	allocator = evcon_allocator_init(static_allocator_buf, sizeof(static_allocator_buf), NULL, evcon_glib_alloc_cb, evcon_glib_free_cb);

	g_atomic_int_set(&lock, 2);

	return (evcon_allocator*) allocator;
}
