#ifndef HELLO_CLIENT
#define HELLO_CLIENT

class Hello_client{
	public:
		Hello_client(char const * name);
		void show(char const * str);
	private:
		char const * name;
};

#endif
