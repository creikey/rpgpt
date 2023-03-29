package codes

import (
 "fmt"
)

type UserCode int


func intPow(n, m int) int {
 if m == 0 {
  return 1
 }
 result := n
 for i := 2; i <= m; i++ {
  result *= n
 }
 return result
}

var numberToChar = [...]rune{'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9'}


func CodeToString(code UserCode) (string, error) {
 toReturn := [4]rune{'A', 'A', 'A', 'A'}

 value := int(code)

 for place := 3; place >= 0; place-- {
  index := 3 - place
  currentPlaceValue := value / intPow(36, place)
  value -= currentPlaceValue * intPow(36, place)
  if currentPlaceValue >= len(numberToChar) { return "", fmt.Errorf("Failed to generate usercode %d to string, currentPlaceValue %d and length of number to char %d in place %d", code, currentPlaceValue, len(numberToChar),place ) }
  toReturn[index] = numberToChar[currentPlaceValue]
 }

 return string(toReturn[:]), nil
}

func ParseUserCode(s string) (UserCode, error) {
 asRune := []rune(s)
 if len(asRune) != 4 { return 0, fmt.Errorf("String to deconvert is not of length 4: %s", s) }
 var toReturn UserCode = 0
 for place := 3; place >= 0; place-- {
  index := 3 - place
  curDigitNum := 0
  found := false
  for i, letter := range numberToChar {
   if letter == asRune[index] {
    curDigitNum = i
    found = true
   }
  }
  if !found { return 0, fmt.Errorf("Failed to find place's number %s", s) }
  toReturn += UserCode(curDigitNum * intPow(36, place))
 }
 return toReturn, nil
}

