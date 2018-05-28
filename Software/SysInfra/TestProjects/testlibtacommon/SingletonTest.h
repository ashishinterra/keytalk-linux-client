#pragma once

#include "ta/singletonholder.hpp"
#include "cxxtest/TestSuite.h"

class SingletonTest : public CxxTest::TestSuite
{
public:
	void testUnique()
	{
		MyTrivialClass::destroy();
		TS_ASSERT_EQUALS(MyTrivialClass::isInstanceExist(), false);
		TS_ASSERT_EQUALS(MyTrivialClass::theNumInstances, 0U);
		MyTrivialClass& myInst1 = MyTrivialClass::instance();
		TS_ASSERT_EQUALS(MyTrivialClass::isInstanceExist(), true);
		TS_ASSERT_EQUALS(MyTrivialClass::theNumInstances, 1U);
		int myVal = 1;
		myInst1.theVal = myVal;
		MyTrivialClass& myInst2 = MyTrivialClass::instance();
		TS_ASSERT_EQUALS(MyTrivialClass::theNumInstances, 1U);
		TS_ASSERT_EQUALS(MyTrivialClass::isInstanceExist(), true);
		TS_ASSERT_EQUALS(myInst2.theVal, myVal);
	}

	void testManualDestruction()
	{
		MyTrivialClass::destroy();
		TS_ASSERT_EQUALS(MyTrivialClass::isInstanceExist(), false);
		MyTrivialClass::instance();
		MyTrivialClass::instance();
		MyTrivialClass::destroy();
		TS_ASSERT_EQUALS(MyTrivialClass::isInstanceExist(), false);
	}

	void testMultipleDestructions()
	{
		MyTrivialClass::destroy();
		TS_ASSERT_EQUALS(MyTrivialClass::isInstanceExist(), false);
		MyTrivialClass& myInst1 = MyTrivialClass::instance();
		MyTrivialClass& myInst2 = MyTrivialClass::instance();
		TS_ASSERT_EQUALS(MyTrivialClass::isInstanceExist(), true);
		myInst1.destroy();
		TS_ASSERT_EQUALS(MyTrivialClass::isInstanceExist(), false);
		myInst2.destroy();
		TS_ASSERT_EQUALS(MyTrivialClass::isInstanceExist(), false);
		myInst1.destroy();
		TS_ASSERT_EQUALS(MyTrivialClass::isInstanceExist(), false);
		myInst2.destroy();
		TS_ASSERT_EQUALS(MyTrivialClass::isInstanceExist(), false);
		MyTrivialClass::destroy();
		TS_ASSERT_EQUALS(MyTrivialClass::isInstanceExist(), false);
		MyTrivialClass::destroy();
		TS_ASSERT_EQUALS(MyTrivialClass::isInstanceExist(), false);
	}

	void testCreationPolicy()
	{
    	MyNonTrivialClass::destroy();
		TS_ASSERT_EQUALS(MyNonTrivialClass::isInstanceExist(), false);
		int myVal = 123;

		MyCreationPolicy<MyNonTrivialClass>::theParam = myVal;
		MyNonTrivialClass& myInst1 = MyNonTrivialClass::instance();
		TS_ASSERT_EQUALS(MyNonTrivialClass::isInstanceExist(), true);
		TS_ASSERT_EQUALS(myInst1.theVal, myVal);

		MyCreationPolicy<MyNonTrivialClass>::theParam = myVal+1;
		MyNonTrivialClass& myInst2 = MyNonTrivialClass::instance();
		TS_ASSERT_EQUALS(myInst1.theVal, myVal);
        TS_ASSERT_EQUALS(myInst2.theVal, myVal);

		MyNonTrivialClass::destroy();
		TS_ASSERT_EQUALS(MyNonTrivialClass::isInstanceExist(), false);

	}
	//
	// What should be actually checked here is the absence of memory leaks caused by non-destroyed MyTrivialClass object.
	// This check works only for Win32 so far (see main()). This method should stay the last in SingletonTest class
	// because it guarantees that nobody else explicitly destroys MyTrivialClass object created in this method.
	//
	void testAutomaticDestruction()
	{
		MyTrivialClass::destroy();
		TS_ASSERT_EQUALS(MyTrivialClass::isInstanceExist(), false);
		MyTrivialClass::instance();
		MyTrivialClass::instance();
		TS_ASSERT_EQUALS(MyTrivialClass::isInstanceExist(), true);
	}
private:
	// Default-constructable class
    class MyTrivialClass: public ta::SingletonHolder<MyTrivialClass>
	{
		friend class ta::SingletonHolder<MyTrivialClass>;
		friend class ta::DefaultCreationPolicy<MyTrivialClass>;
	public:
		int theVal;
		static unsigned int theNumInstances;
	private:
		MyTrivialClass():theVal(0) { ++theNumInstances; }
		~MyTrivialClass()          { --theNumInstances; }
	};

	// Class without defalt c'tor: use creation policy to pass arguments
	template<class T>
		class MyCreationPolicy
		{
		public:
			static T* createInstance() 	{ return new T(theParam); }
			static int theParam;
		};

	class MyNonTrivialClass: public ta::SingletonHolder<MyNonTrivialClass, MyCreationPolicy<MyNonTrivialClass> >
	{
		friend class ta::SingletonHolder<MyNonTrivialClass, MyCreationPolicy<MyNonTrivialClass> >;
		friend class MyCreationPolicy<MyNonTrivialClass>;
	public:
		int theVal;
	private:
		MyNonTrivialClass(int aVal): theVal(aVal) {}
		~MyNonTrivialClass()                      {}
	};
};

template<class T> int SingletonTest::MyCreationPolicy<T>::theParam;
