#pragma once

#include <uv.h>

struct Async {
	uv_async_t uv_async;

	Async(uv_loop_t* loop) {
		uv_async.loop = loop;
	}

	void start(void(*cb)(Async *)) {
		uv_async_init(uv_async.loop, &uv_async, (uv_async_cb)cb);
	}

	void send() {
		uv_async_send(&uv_async);
	}

	void close() {
		uv_close((uv_handle_t *)&uv_async, [](uv_handle_t *a) {
			delete (Async *)a;
		});
	}

	void setData(void *data) {
		uv_async.data = data;
	}

	void *getData() {
		return uv_async.data;
	}
};