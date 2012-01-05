#include <iostream>

#include "gc.hpp"

extern "C"
int atexit ( void ( * function ) (void) );

using namespace std;
using namespace gc;

class A;
class B;

class A : public gc::object
{
public:
	A()
	{
		cout << "A()" << endl;
	}

	~A()
	{
		cout << "~A()" << endl;
	}


	handle<B>	b;
};

class B : public gc::object
{
public:
	B()
	{
		cout << "B()" << endl;
	}

	~B()
	{
		cout << "~B()" << endl;
	}

	handle<A>	a;
};

int main(int argc, char *argv[])
{
	cout << "Test 1" << endl;

	{
		gc::handle<B>	bptr(new B());
		gc::handle<A>	aptr(new A());

		aptr->b = bptr;
		bptr->a	= aptr;
	}

	gc::mark_and_sweep();
	gc::mark_and_sweep();

	atexit(gc::mark_and_sweep);
	return 0;

}
