package codes

import (
    "testing"
    "runtime"
)

func assert(t *testing.T, cond bool) {
 if !cond {
  _, _, line, _ := runtime.Caller(1)
  t.Fatalf("Failed on line %d", line)
 }
}

func TestCodes(t *testing.T) {
 parsed, err := ParseUserCode("AAAA")
 assert(t, err == nil)
 assert(t, int(parsed) == 0)

 var stringed string
 stringed, err = CodeToString(UserCode(1))
 assert(t, err == nil)
 assert(t, stringed == "AAAB")

 parsed, err = ParseUserCode("AAAB")
 assert(t, err == nil)
 assert(t, int(parsed) == 1)

 parsed, err = ParseUserCode("BAAA")
 assert(t, err == nil)
 assert(t, int(parsed) == 46656)

}

