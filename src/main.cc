#include <algorithm>
#include <iostream>
#include <functional>
#include <utility>
#include <vector>

template <typename... Args>
class Wrapper {
public:
  template <typename Cls> using MemberFuncT      = void (Cls::*)(Args...);
  template <typename Cls> using ConstMemberFuncT = void (Cls::*)(Args...) const;
                          using FuncT            = void      (*)(Args...);

  /// Plop is a class to essentially bind a member function to an instance so
  /// it can be treated as a regular function call.
  template <typename Obj, typename Func>
  struct Plop {
    Plop (Obj &_obj, Func _func) : obj(_obj), func(_func) {}

    void Call (Args... args) {
      (obj.*func)(args...);
    }

    Obj &obj;
    Func func;
  };

  // Figure out how many bytes are required to hold pointers for an object
  // and a member function.
  struct Empty {};
  static constexpr size_t STORAGE = sizeof(Plop<Empty, MemberFuncT<Empty>>);

  /// Constructor for a plain old function.
  Wrapper(FuncT func) :
      fn_data_(func),
      caller_(&Wrapper::Caller)
  {}

  /// Constructor for an object and member function.
  template <typename Obj, typename Cls,
            typename PlopT=Plop<Obj, MemberFuncT<Cls>>>
  Wrapper(Obj &obj, MemberFuncT<Cls> func) :
      fn_data_(obj, func),
      caller_(&Wrapper::MemberCaller<PlopT>)
  {
    static_assert(sizeof(PlopT) <= STORAGE, "PlopT too large");

    // Placement new to construct necessary references
    //new(fn_data_.plop_storage_) PlopT(obj, func);
  }

  /// Constructor for an object and const member function.
  template <typename Obj, typename Cls,
            typename PlopT=Plop<Obj, ConstMemberFuncT<Cls>>>
  Wrapper(Obj &obj, ConstMemberFuncT<Cls> func) :
      fn_data_(obj, func),
      caller_(&Wrapper::MemberCaller<PlopT>)
  {
    static_assert(sizeof(PlopT) <= STORAGE, "PlopT too large");

    // Placement new to construct necessary references
    //new(fn_data_.plop_storage_) PlopT(obj, func);
  }

  /// Call operator that will forward the args on to the appropriate function
  /// depending on whether we're wrapping a free function or a member
  /// function.
  void operator() (Args... args) {
    caller_(fn_data_, args...);
  }

private:
  union FnData {
    FuncT func_;
    uint8_t plop_storage_[STORAGE];

    /// Constructor that just stores a pointer to a function.
    FnData (FuncT f) : func_(f) {}

    /// Constructor that uses placement new to create the appropriate
    /// contents.
    template <typename Obj, typename Func>
    FnData (Obj &obj, Func func) {
      new(plop_storage_) Plop<Obj, Func>(obj, func);
    }
  } fn_data_;

  void (*caller_)(FnData &, Args...);

  static void Caller (FnData &fn_data, Args... args) {
    fn_data.func_(args...);
  }

  template <typename Plop>
  static void MemberCaller (FnData &fn_data, Args... args) {
    auto plop = reinterpret_cast<Plop*>(fn_data.plop_storage_);
    plop->Call(args...);
  }
};


template <typename... Args>
class Signal {
public:
  using WrapperT = Wrapper<Args...>;
  using HandleT = size_t;

  HandleT Connect(WrapperT &&slot) {
    HandleT h = handle_counter_;
    ++handle_counter_;
    slots_.push_back(std::make_pair(h, slot));
    return h;
  }

  HandleT Connect(typename WrapperT::FuncT func) {
    return Connect(WrapperT(func));
  }

  template <typename Obj, typename Cls>
  HandleT Connect(Obj &obj,
                  typename WrapperT::template MemberFuncT<Cls> func) {
    return Connect(WrapperT(obj, func));
  }

  template <typename Obj, typename Cls>
  HandleT Connect(Obj &obj,
                  typename WrapperT::template ConstMemberFuncT<Cls> func) {
    return Connect(WrapperT(obj, func));
  }

  void Emit (Args... args) {
    for (auto& slot_pair : slots_) {
      slot_pair.second(args...);
    }
  }

  void operator() (Args... args) {
    Emit(args...);
  }

  bool Disconnect (HandleT handle) {
    bool found = false;
    auto end = std::remove_if(slots_.begin(), slots_.end(),
                              [&](const auto& kv) {
                                found |= kv.first == handle;
                                return kv.first == handle;
                              });
    slots_.erase(end, slots_.end());
    return found;
  }

  size_t Size () const { return slots_.size(); }
  void Clear () { slots_; }

private:
  static HandleT handle_counter_;

  std::vector<std::pair<HandleT, WrapperT>> slots_;
};

template <typename... Args>
typename Signal<Args...>::HandleT Signal<Args...>::handle_counter_ = 0;


void f (int a, int b, int *c) {
  *c = a + b;
}

class F {
public:
  F (int d) : d_(d) {}

  void f (int a, int b, int *c) {
    *c = a+b * d_;
  }

private:
  int d_;
};

void Slot1 (int a) {
  std::cout << "called Slot1(" << a << ")\n";
}

class SlotClass {
public:
  void Slot2 (int a) {
    std::cout << "called SlotClass::Slot2(" << a << ")\n";
  }
};

class Button {
public:
  Signal<> clicked;

  void ClickMe (void) {
    clicked.Emit();
  }
};

class Message {
public:
  virtual void ShowMessage (void) const {
    std::cout << "You have clicked the button\n";
  }
};

class MessageDerived : public Message {
public:
  void ShowMessage (void) const override {
    std::cout << "You have clicked the button (derived)\n";
  }

  static void StaticMessaage (void) {
    std::cout << "Call from static method\n";
  }
};

int main(int argc, char **argv) {
  // DELETE THESE.  Used to suppress unused variable warnings.
  (void)argc;
  (void)argv;

  std::cout << "Hello, World!" << std::endl;

  using WrapperT = Wrapper<int, int, int*>;

  WrapperT w1(f);

  int result = 0;
  w1(10, 20, &result);
  std::cout << "w1 result = " << result << "\n";

  F fobj(2);
  WrapperT w2(fobj, &F::f);
  w2(10, 20, &result);
  std::cout << "w2 result = " << result << "\n";

  SlotClass slotter;

  Signal<int> signal1;
  auto handle1 = signal1.Connect(Slot1);
  auto handle2 = signal1.Connect(slotter, &SlotClass::Slot2);
  signal1.Emit(10);
  signal1.Disconnect(handle2);
  signal1.Emit(20);
  signal1.Disconnect(handle1);
  signal1.Emit(30);

  Button button;
  Message message;
  MessageDerived message2;
  button.clicked.Connect(message, &Message::ShowMessage);
  button.clicked.Connect(message2, &Message::ShowMessage);
  button.clicked.Connect(&MessageDerived::StaticMessaage);
  button.ClickMe();

  return 0;
}
