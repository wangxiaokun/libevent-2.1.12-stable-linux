#include <event2/event.h>
#include <event2/listener.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <string.h>
#include <stdlib.h>

#ifndef _WIN32
#include <signal.h>
#else
#endif

static
enum bufferevent_filter_result filter_in(
    struct evbuffer *s,
    struct evbuffer *d,
    ev_ssize_t limit,
	enum bufferevent_flush_mode mode,void *arg)
{
	char data[1024];
	memset(data, 0, sizeof(data));

	//读取并清理原数据
	int len;
	len = evbuffer_remove(s, data, sizeof(data)-1);

	//所有字母转成大写
	int i = 0;
	for (i = 0; i < len; ++i)
	{
		data[i] = toupper(data[i]);
	}

	evbuffer_add(d, data, len);

	return BEV_OK;
}

static
enum bufferevent_filter_result filter_out(
    struct  evbuffer *s,
    struct  evbuffer *d,
    ev_ssize_t limit,
	enum bufferevent_flush_mode mode,void *arg)
{
	char data[1024] = {0};
	//读取并清理原数据
	int len = evbuffer_remove(s, data, sizeof(data)-1);

    const char* pad = "================\n";
    evbuffer_add(d, pad, strlen(pad));
	evbuffer_add(d, data, len);
    evbuffer_add(d, pad, strlen(pad));

	return BEV_OK;
}

static
void read_cb(struct  bufferevent*bev, void *arg)
{
    printf("read_cb\n");

	char data[1024] = {0};
	int len = bufferevent_read(bev, data, sizeof(data)-1);

	//回复客户消息，经过输出过滤
	bufferevent_write(bev,data,len);

}

static
void write_cb(struct bufferevent*bev, void *arg)
{
    printf("write_cb\n");
}

static
void event_cb(struct bufferevent*bev, short events, void *arg)
{
    printf("event_cb\n");
}

static
void listen_cb(struct evconnlistener *ev,evutil_socket_t s, struct sockaddr* sin, int slen, void *arg)
{
	struct event_base *base = (struct event_base*)arg;
	//创建bufferevent
	struct bufferevent *bev = bufferevent_socket_new(base,s,BEV_OPT_CLOSE_ON_FREE);

	//绑定bufferevent filter
	struct bufferevent * bev_filter = bufferevent_filter_new(
		bev,	//指向要被包装的底层Bufferevent
		filter_in,//输入过滤函数
		filter_out,//输出过滤函数
		BEV_OPT_CLOSE_ON_FREE, //关闭filter是同时关闭bufferevent
		0,		//清理的回调函数
		0		//传递给回调的参数
	);


	//设置bufferevent的回调
	bufferevent_setcb(bev_filter, read_cb, write_cb, event_cb, NULL);//回调函数的参数
	bufferevent_enable(bev_filter,EV_READ|EV_WRITE);
}

int main(int argc,char *argv[])
{

#ifdef _WIN32
	//初始化socket库
	WSADATA wsa;
	WSAStartup(MAKEWORD(2,2),&wsa);
#else
	//忽略管道信号，发送数据给已关闭的socket
	if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
		return 1;
#endif

	//创建libevent的上下文
	struct event_base * base = event_base_new();
	if (base == NULL) {
        printf("event_base_new failed\n");
        exit(1);
	}
	//监听端口
	//socket ，bind，listen 绑定事件
	struct sockaddr_in sin;
	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(5001);

	struct evconnlistener *ev = evconnlistener_new_bind(base,	// libevent的上下文
		listen_cb,					//接收到连接的回调函数
		base,						//回调函数获取的参数 arg
		LEV_OPT_REUSEABLE|LEV_OPT_CLOSE_ON_FREE,//地址重用，evconnlistener关闭同时关闭socket
		10,							//连接队列大小，对应listen函数
		(struct sockaddr*)&sin,							//绑定的地址和端口
		sizeof(sin)
		);

	//事件分发处理
	if(base)
		event_base_dispatch(base);
	if(ev)
		evconnlistener_free(ev);
	if(base)
		event_base_free(base);

	return 0;
}
