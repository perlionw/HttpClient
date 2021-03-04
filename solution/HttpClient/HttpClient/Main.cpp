#include <string.h>
#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/http.h>
#include <event2/http_struct.h>
#include <iostream>

struct download_context
{
	struct evhttp_uri *uri;
	struct event_base *base;
	struct evhttp_connection *cn;
	struct evhttp_request *req;
	struct evbuffer *buffer;
	int ok;
};

static void download_callback(struct evhttp_request *req, void *arg);

static int download_renew_request(struct download_context *ctx);

static void download_callback(struct evhttp_request *req, void *arg)
{
	struct download_context *ctx = (struct download_context *)arg;
	struct evhttp_uri *new_uri = NULL;
	const char *new_location = NULL;

	/*  response is ready */
	if (req)
	{
		switch (req->response_code)
		{
			case HTTP_OK:
				/*
				*           * Response is received. No futher handling is required.
				*                   * Finish
				*                           */
				event_base_loopexit(ctx->base, 0);
				break;

			case HTTP_MOVEPERM:
			case HTTP_MOVETEMP:
				new_location = evhttp_find_header(req->input_headers, "Location");
				if (!new_location)
					return;

				new_uri = evhttp_uri_parse(new_location);
				if (!new_uri)
					return;

				evhttp_uri_free(ctx->uri);
				ctx->uri = new_uri;

				download_renew_request(ctx);
				return;

			default:
				/*  FAILURE */
				event_base_loopexit(ctx->base, 0);
				return;
		}

		evbuffer_add_buffer(ctx->buffer, req->input_buffer);

		/*  SUCCESS */
		ctx->ok = 1;
	}
}

struct download_context *context_new(const char *url)
{
	struct download_context *ctx = 0;
	ctx = new (struct download_context);//(struct download_context*)calloc(0, sizeof(*ctx));
	memset(ctx, 0, sizeof(download_context));
	if (!ctx)
		return 0;

	ctx->uri = evhttp_uri_parse(url);
	if (!ctx->uri)
		return 0;

	ctx->base = event_base_new();
	if (!ctx->base)
		return 0;

	ctx->buffer = evbuffer_new();

	download_renew_request(ctx);

	return ctx;
}

void context_free(struct download_context *ctx)
{
	evhttp_connection_free(ctx->cn);
	event_base_free(ctx->base);

	if (ctx->buffer)
		evbuffer_free(ctx->buffer);

	evhttp_uri_free(ctx->uri);
	free(ctx);
}

static int download_renew_request(struct download_context *ctx)
{
	/*  free connections & request */
	if (ctx->cn)
		evhttp_connection_free(ctx->cn);

	ctx->cn = evhttp_connection_base_new(ctx->base, NULL, "127.0.0.1", 8081);

	ctx->req = evhttp_request_new(download_callback, ctx);

	evhttp_make_request(ctx->cn, ctx->req, EVHTTP_REQ_GET, "/station/robotVideoList?robotId=1");
	//evbuffer_add(ctx->req->output_buffer, )
	evhttp_add_header(ctx->req->output_headers, "ContentType", "application/json;charset=UTF-8");
	return 0;
}

struct evbuffer *download_url(const char *url)
{
	/*  setup request, connection etc */

	struct download_context *ctx = context_new(url);
	if (!ctx)
		return 0;

	/*  do all of the job */
	event_base_dispatch(ctx->base);

	struct evbuffer *retval = 0;
	if (ctx->ok)
	{
		retval = ctx->buffer;
		ctx->buffer = 0;
	}

	context_free(ctx);

	return retval;
}

int main(int argc, char *argv[])
{

#ifdef WIN32
	WORD wVersionRequested;
	WSADATA wsaData;
	wVersionRequested = MAKEWORD(2, 2);
	(void)WSAStartup(wVersionRequested, &wsaData);
#endif

	//if (argc < 2)
	//{
	//	printf("usage: %s example.com/\n", argv[0]);
	//	return 1;
	//}

	struct evbuffer *data = download_url(argv[1]);

	printf("got %d bytes\n", data ? evbuffer_get_length(data) : -1);

	if (data)
	{
		int len = evbuffer_get_length(data);
		unsigned char *joined = evbuffer_pullup(data, len);
		printf("data itself:\n====================\n");
		joined[len] = '\0';
		std::cout << joined << std::endl;
		printf("\n====================\n");
		
		int len1 = evbuffer_get_length(data);
		evbuffer_free(data);
	}

	system("pause");
	return 0;
}