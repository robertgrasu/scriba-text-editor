-- a comment
local function fib(n)
  if n < 2 then return n end
  return fib(n - 1) + fib(n - 2)
end
local t = { name = "x", 42, true }
print(fib(10), #t)
