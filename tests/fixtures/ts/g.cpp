#include <vector>
template <typename T> class Box {
public:
  explicit Box(T v) : v_(v) {}
  const T& get() const { return v_; }
private:
  T v_;
};
int main() { std::vector<int> a{1, 2, 3}; Box<int> b(a[0]); return b.get() > 0 ? 0 : 1; }
