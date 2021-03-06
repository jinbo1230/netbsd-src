// RUN: %clang_analyze_cc1 -analyzer-checker=core,debug.ExprInspection -std=c++11 -verify %s
// RUN: %clang_analyze_cc1 -analyzer-checker=core,debug.ExprInspection -std=c++17 -verify %s
// RUN: %clang_analyze_cc1 -analyzer-checker=core,debug.ExprInspection -std=c++11 -analyzer-config elide-constructors=false -DNO_ELIDE_FLAG -verify %s
// RUN: %clang_analyze_cc1 -analyzer-checker=core,debug.ExprInspection -std=c++17 -analyzer-config elide-constructors=false -DNO_ELIDE_FLAG -verify %s

// Copy elision always occurs in C++17, otherwise it's under
// an on-by-default flag.
#if __cplusplus >= 201703L
  #define ELIDE 1
#else
  #ifndef NO_ELIDE_FLAG
    #define ELIDE 1
  #endif
#endif

void clang_analyzer_eval(bool);

namespace variable_functional_cast_crash {

struct A {
  A(int) {}
};

void foo() {
  A a = A(0);
}

struct B {
  A a;
  B(): a(A(0)) {}
};

} // namespace variable_functional_cast_crash


namespace ctor_initializer {

struct S {
  int x, y, z;
};

struct T {
  S s;
  int w;
  T(int w): s(), w(w) {}
};

class C {
  T t;
public:
  C() : t(T(4)) {
    S s = {1, 2, 3};
    t.s = s;
    // FIXME: Should be TRUE regardless of copy elision.
    clang_analyzer_eval(t.w == 4);
#ifdef ELIDE
    // expected-warning@-2{{TRUE}}
#else
    // expected-warning@-4{{UNKNOWN}}
#endif
  }
};


struct A {
  int x;
  A(): x(0) {}
  ~A() {}
};

struct B {
  A a;
  B() : a(A()) {}
};

void foo() {
  B b;
  clang_analyzer_eval(b.a.x == 0); // expected-warning{{TRUE}}
}

} // namespace ctor_initializer


namespace elision_on_ternary_op_branches {
class C1 {
  int x;
public:
  C1(int x): x(x) {}
  int getX() const { return x; }
  ~C1();
};

class C2 {
  int x;
  int y;
public:
  C2(int x, int y): x(x), y(y) {}
  int getX() const { return x; }
  int getY() const { return y; }
  ~C2();
};

void foo(int coin) {
  C1 c1 = coin ? C1(1) : C1(2);
  if (coin) {
    clang_analyzer_eval(c1.getX() == 1); // expected-warning{{TRUE}}
  } else {
    clang_analyzer_eval(c1.getX() == 2); // expected-warning{{TRUE}}
  }
  C2 c2 = coin ? C2(3, 4) : C2(5, 6);
  if (coin) {
    clang_analyzer_eval(c2.getX() == 3); // expected-warning{{TRUE}}
    clang_analyzer_eval(c2.getY() == 4); // expected-warning{{TRUE}}
  } else {
    clang_analyzer_eval(c2.getX() == 5); // expected-warning{{TRUE}}
    clang_analyzer_eval(c2.getY() == 6); // expected-warning{{TRUE}}
  }
}
} // namespace elision_on_ternary_op_branches


namespace address_vector_tests {

template <typename T> struct AddressVector {
  T *buf[10];
  int len;

  AddressVector() : len(0) {}

  void push(T *t) {
    buf[len] = t;
    ++len;
  }
};

class ClassWithoutDestructor {
  AddressVector<ClassWithoutDestructor> &v;

public:
  ClassWithoutDestructor(AddressVector<ClassWithoutDestructor> &v) : v(v) {
    v.push(this);
  }

  ClassWithoutDestructor(ClassWithoutDestructor &&c) : v(c.v) { v.push(this); }
  ClassWithoutDestructor(const ClassWithoutDestructor &c) : v(c.v) {
    v.push(this);
  }
};

ClassWithoutDestructor make1(AddressVector<ClassWithoutDestructor> &v) {
  return ClassWithoutDestructor(v);
}
ClassWithoutDestructor make2(AddressVector<ClassWithoutDestructor> &v) {
  return make1(v);
}
ClassWithoutDestructor make3(AddressVector<ClassWithoutDestructor> &v) {
  return make2(v);
}

void testMultipleReturns() {
  AddressVector<ClassWithoutDestructor> v;
  ClassWithoutDestructor c = make3(v);

#if ELIDE
  clang_analyzer_eval(v.len == 1); // expected-warning{{TRUE}}
  clang_analyzer_eval(v.buf[0] == &c); // expected-warning{{TRUE}}
#else
  clang_analyzer_eval(v.len == 5); // expected-warning{{TRUE}}
  clang_analyzer_eval(v.buf[0] != v.buf[1]); // expected-warning{{TRUE}}
  clang_analyzer_eval(v.buf[1] != v.buf[2]); // expected-warning{{TRUE}}
  clang_analyzer_eval(v.buf[2] != v.buf[3]); // expected-warning{{TRUE}}
  clang_analyzer_eval(v.buf[3] != v.buf[4]); // expected-warning{{TRUE}}
  clang_analyzer_eval(v.buf[4] == &c); // expected-warning{{TRUE}}
#endif
}

class ClassWithDestructor {
  AddressVector<ClassWithDestructor> &v;

public:
  ClassWithDestructor(AddressVector<ClassWithDestructor> &v) : v(v) {
    v.push(this);
  }

  ClassWithDestructor(ClassWithDestructor &&c) : v(c.v) { v.push(this); }
  ClassWithDestructor(const ClassWithDestructor &c) : v(c.v) {
    v.push(this);
  }

  ~ClassWithDestructor() { v.push(this); }
};

void testVariable() {
  AddressVector<ClassWithDestructor> v;
  {
    ClassWithDestructor c = ClassWithDestructor(v);
    // Check if the last destructor is an automatic destructor.
    // A temporary destructor would have fired by now.
#if ELIDE
    clang_analyzer_eval(v.len == 1); // expected-warning{{TRUE}}
#else
    clang_analyzer_eval(v.len == 3); // expected-warning{{TRUE}}
#endif
  }
#if ELIDE
  // 0. Construct the variable.
  // 1. Destroy the variable.
  clang_analyzer_eval(v.len == 2); // expected-warning{{TRUE}}
  clang_analyzer_eval(v.buf[0] == v.buf[1]); // expected-warning{{TRUE}}
#else
  // 0. Construct the temporary.
  // 1. Construct the variable.
  // 2. Destroy the temporary.
  // 3. Destroy the variable.
  clang_analyzer_eval(v.len == 4); // expected-warning{{TRUE}}
  clang_analyzer_eval(v.buf[0] == v.buf[2]); // expected-warning{{TRUE}}
  clang_analyzer_eval(v.buf[1] == v.buf[3]); // expected-warning{{TRUE}}
#endif
}

struct TestCtorInitializer {
  ClassWithDestructor c;
  TestCtorInitializer(AddressVector<ClassWithDestructor> &v)
    : c(ClassWithDestructor(v)) {}
};

void testCtorInitializer() {
  AddressVector<ClassWithDestructor> v;
  {
    TestCtorInitializer t(v);
    // Check if the last destructor is an automatic destructor.
    // A temporary destructor would have fired by now.
#if ELIDE
    clang_analyzer_eval(v.len == 1); // expected-warning{{TRUE}}
#else
    clang_analyzer_eval(v.len == 3); // expected-warning{{TRUE}}
#endif
  }
#if ELIDE
  // 0. Construct the member variable.
  // 1. Destroy the member variable.
  clang_analyzer_eval(v.len == 2); // expected-warning{{TRUE}}
  clang_analyzer_eval(v.buf[0] == v.buf[1]); // expected-warning{{TRUE}}
#else
  // 0. Construct the temporary.
  // 1. Construct the member variable.
  // 2. Destroy the temporary.
  // 3. Destroy the member variable.
  clang_analyzer_eval(v.len == 4); // expected-warning{{TRUE}}
  clang_analyzer_eval(v.buf[0] == v.buf[2]); // expected-warning{{TRUE}}
  clang_analyzer_eval(v.buf[1] == v.buf[3]); // expected-warning{{TRUE}}
#endif
}


ClassWithDestructor make1(AddressVector<ClassWithDestructor> &v) {
  return ClassWithDestructor(v);
}
ClassWithDestructor make2(AddressVector<ClassWithDestructor> &v) {
  return make1(v);
}
ClassWithDestructor make3(AddressVector<ClassWithDestructor> &v) {
  return make2(v);
}

void testMultipleReturnsWithDestructors() {
  AddressVector<ClassWithDestructor> v;
  {
    ClassWithDestructor c = make3(v);
    // Check if the last destructor is an automatic destructor.
    // A temporary destructor would have fired by now.
#if ELIDE
    clang_analyzer_eval(v.len == 1); // expected-warning{{TRUE}}
#else
    clang_analyzer_eval(v.len == 9); // expected-warning{{TRUE}}
#endif
  }

#if ELIDE
  // 0. Construct the variable. Yes, constructor in make1() constructs
  //    the variable 'c'.
  // 1. Destroy the variable.
  clang_analyzer_eval(v.len == 2); // expected-warning{{TRUE}}
  clang_analyzer_eval(v.buf[0] == v.buf[1]); // expected-warning{{TRUE}}
#else
  // 0. Construct the temporary in make1().
  // 1. Construct the temporary in make2().
  // 2. Destroy the temporary in make1().
  // 3. Construct the temporary in make3().
  // 4. Destroy the temporary in make2().
  // 5. Construct the temporary here.
  // 6. Destroy the temporary in make3().
  // 7. Construct the variable.
  // 8. Destroy the temporary here.
  // 9. Destroy the variable.
  clang_analyzer_eval(v.len == 10); // expected-warning{{TRUE}}
  clang_analyzer_eval(v.buf[0] == v.buf[2]); // expected-warning{{TRUE}}
  clang_analyzer_eval(v.buf[1] == v.buf[4]); // expected-warning{{TRUE}}
  clang_analyzer_eval(v.buf[3] == v.buf[6]); // expected-warning{{TRUE}}
  clang_analyzer_eval(v.buf[5] == v.buf[8]); // expected-warning{{TRUE}}
  clang_analyzer_eval(v.buf[7] == v.buf[9]); // expected-warning{{TRUE}}
#endif
}
} // namespace address_vector_tests
