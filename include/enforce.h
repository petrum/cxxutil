#ifndef ENFORCE_H_
#define ENFORCE_H_
/////////////////////////////////////////////////////////////////////
// enforce.h - Assert-like error handler                           //
//                                                                 //
// Jim Fawcett, CSE687 - Object Oriented Design, Spring 2005       //
// Source: Andrei Alexandrescu and Petru Marginean                 //
//         Enforcements, C/C++ User's Journal, June 2003           //
/////////////////////////////////////////////////////////////////////
// Note:                                                           //
// I have simply made some cosmetic changes.  The code is due,     //
// in its entirety, to the efforts of Andrei and Petru.            //
/////////////////////////////////////////////////////////////////////

#include <string>
#include <sstream>
#include <stdexcept>

//----< default Predicate Policy for detecting faults >--------------

struct DefaultPredicate
{
  template <class T>
  static bool Wrong(const T& obj)
  {
    return !obj;
  }
};

//----< default Raise Policy for throwing exceptions >---------------

struct DefaultRaiser
{
  template <class T>
  static void Throw(
    const T&, 
    const std::string& message, 
    const char* locus
  )
  {
    throw std::runtime_error(message + '\n' + locus);
  }
};

//
/////////////////////////////////////////////////////////////////////
// Enforcer class

template<typename Ref,    // Reference Type - decision state 
         typename P,      // Predicate Policy
         typename R       // Raise Event Policy
>
class Enforcer
{
public:
  
  // If Wrong(t) then locus_ is non-null, indicating error
  // locus_ provides information about the error locale

  Enforcer(Ref t, const char* locus) 
    : t_(t), locus_(P::Wrong(t) ? locus : 0) {}

  // Here is where exception is thrown

  Ref operator*() const
  {
    if (locus_) R::Throw(t_, msg_, locus_);
    return t_;
  }
  // Designer can format message here

  template <class MsgType>
  Enforcer& operator()(const MsgType& msg)
  {
    if (locus_) 
    {
      // Here we have time; an exception will be thrown
      std::ostringstream ss;
      ss << msg;
      msg_ += ss.str();
    }
    return *this;
  }

private:
  Ref t_;
  std::string msg_;
  const char* const locus_;
};

//
//----< create Enforcer from const decision object >-----------------

template <class P, class R, typename T>
inline Enforcer<const T&, P, R> 
MakeEnforcer(const T& t, const char* locus)
{
  return Enforcer<const T&, P, R>(t, locus);
}
//----< create Enforcer from non-const decision object >-------------

template <class P, class R, typename T>
inline Enforcer<T&, P, R> 
MakeEnforcer(T& t, const char* locus)
{
  return Enforcer<T&, P, R>(t, locus);
}

//----< ENFORCE macro creates Enforcer with locale information >-----
//
// This macro does the following things:
// 1. creates an Enforcer object
// 2. applies operator() to it
// 3. applies operator* to it

// The following macros convert decision expression into a string

#define STRINGIZE(expr) STRINGIZE_HELPER(expr)
#define STRINGIZE_HELPER(expr) #expr

// The * prepended to created Enforcer object invokes operator*()
// resulting in the enforcement action, if any
// The (exp) cause operator() to be invoked which appends exp
// to Enforcer's msg_ or does nothing, depending on the decision.

#define ENFORCE(exp) \
  *MakeEnforcer<DefaultPredicate,\
  DefaultRaiser>((exp), "Expression '" #exp "' failed in '" \
  __FILE__ "', line: " STRINGIZE(__LINE__))

#endif
