#include <algorithm>
#include <iostream>
#include <functional>
#include <utility>
#include <vector>

template <typename... Args>
class Wrapper {
public:
  static constexpr size_t STORAGE = 3*sizeof(void*);

  template <typename Cls>
  using MemberFuncT = void (Cls::*)(Args...);
  using FuncT = void(*)(Args...);

  template <typename Obj, typename Func>
  struct Plop {

    using FuncT = Func;

    Plop (Obj &_obj, FuncT _func) :
        obj(_obj),
        func(_func)
    {}

    void Call (Args... args) {
      (obj.*func)(args...);
    }

    Obj &obj;
    FuncT func;
  };

  explicit Wrapper(FuncT func) :
      fn_data_((void*)func),
      caller_(&Wrapper::Caller)
  {}

  template <typename Obj, typename Cls,
            typename PlopT=Plop<Obj, MemberFuncT<Cls>>>
  Wrapper(Obj &obj, MemberFuncT<Cls> func) :
    caller_(&Wrapper::MemberCaller<PlopT>)
  {
    static_assert(sizeof(PlopT) <= STORAGE, "PlopT too large");

    // Placement new to construct necessary references
    new(fn_data_.plop_storage_) PlopT(obj, func);
  }

  void Caller (Args... args) {
    auto func = reinterpret_cast<FuncT>(fn_data_.func_);
    func(args...);
  }

  template <typename Plop>
  void MemberCaller (Args... args) {
    auto plop = reinterpret_cast<Plop*>(fn_data_.plop_storage_);
    plop->Call(args...);
  }

  void operator() (Args... args) {
    (this->*caller_)(args...);
  }

private:
  union FnData {
    void *func_;
    uint8_t plop_storage_[STORAGE];
    FnData (void *f) : func_(f) {}
    FnData () {}
  };

  FnData fn_data_;

  void (Wrapper::*caller_)(Args...);
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
  HandleT Connect(Obj &obj, typename WrapperT::template MemberFuncT<Cls> func) {
    return Connect(WrapperT(obj, func));
  }

  void Emit (Args... args) {
    for (auto& slot_pair : slots_) {
      slot_pair.second(args...);
    }
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

  return 0;
}
