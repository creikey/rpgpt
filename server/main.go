package main

import (
 "fmt"
 "time"
 "net/http"
 "io"
 "os"
 "context"
 "log"
 "io/ioutil"
 "strings"
 "encoding/json"
 "math/rand"
 gogpt "github.com/sashabaranov/go-gpt3"
 "github.com/stripe/stripe-go/v74"
 "github.com/stripe/stripe-go/v74/webhook"
 "github.com/stripe/stripe-go/v74/checkout/session"

 "gorm.io/gorm"
 "gorm.io/driver/sqlite"

)

// BoughtType values. do not reorganize these or you fuck up the database
const (
 DayPass = iota
)

const (
 // A-Z and 0-9, four digits means this many codes
 MaxCodes = 36 * 36 * 36 * 36
)

type userCode int

type User struct {
 CreatedAt time.Time
 UpdatedAt time.Time
 DeletedAt gorm.DeletedAt `gorm:"index"`

 Code  userCode `gorm:"primaryKey"` // of maximum value max codes, incremented one by one. These are converted to 4 digit alphanumeric code users can remember/use
 BoughtTime int64 // unix time. Used to figure out if the pass is still valid
 BoughtType int // enum

 IsFulfilled bool // before users are checked out they are unfulfilled
 CheckoutSessionID string
}

var c *gogpt.Client
var logResponses = false
var doCors = false
var checkoutRedirectTo string
var daypassPriceId string
var webhookSecret string
var db *gorm.DB

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

func codeToString(code userCode) (string, error) {
 toReturn := ""
 value := int(code)
 // converting to base 36 (A-Z + numbers) then into characters, then appending
 for digit := 3; digit >= 0; digit-- {
  currentPlaceValue := value / intPow(36, digit)
  value -= currentPlaceValue * intPow(36, digit)
  if currentPlaceValue >= len(numberToChar) { return "", fmt.Errorf("Failed to generate usercode %d to string, currentPlaceValue %d and length of number to char %d", code, currentPlaceValue, len(numberToChar)) }
  toReturn += string(numberToChar[currentPlaceValue])
 }
 return toReturn, nil;
}

func parseUserCode(s string) (userCode, error) {
 asRune := []rune(s)
 if len(asRune) != 4 { return 0, fmt.Errorf("String to deconvert is not of length 4: %s", s) }
 var toReturn userCode = 0
 for digit := 3; digit >= 0; digit-- {
  curDigitNum := 0
  found := false
  for i, letter := range numberToChar {
   if letter == asRune[digit] {
    curDigitNum = i
    found = true
   }
  }
  if !found { return 0, fmt.Errorf("Failed to find digit's number %s", s) }
  toReturn += userCode(curDigitNum * intPow(36, digit))
 }
 return toReturn, nil
}

func isUserOld(user User) bool {
 return (currentTime() - user.BoughtTime) > 24*60*60
}

func clearOld(db *gorm.DB) {
 var users []User
 result := db.Find(&users)
 if result.Error != nil {
  log.Fatal(result.Error)
 }
 var toDelete []userCode // codes
 for _, user := range users {
  if user.BoughtType != 0 {
   panic("Don't know how to handle bought type " + string(user.BoughtType) + " yet")
  }
  if isUserOld(user) {
   toDelete = append(toDelete, user.Code)
  }
 }

 for _, del := range toDelete {
  db.Delete(&User{}, del)
 }
}

func webhookResponse(w http.ResponseWriter, req *http.Request) {
 const MaxBodyBytes = int64(65536)
 req.Body = http.MaxBytesReader(w, req.Body, MaxBodyBytes)

 body, err := ioutil.ReadAll(req.Body)
 if err != nil {
  log.Printf("Error reading request body: %v\n", err)
  w.WriteHeader(http.StatusServiceUnavailable)
  return
 }

 endpointSecret := webhookSecret
 event, err := webhook.ConstructEvent(body, req.Header.Get("Stripe-Signature"), endpointSecret)

 if err != nil {
  log.Printf("Error verifying webhook signature %s\n", err)
  w.WriteHeader(http.StatusBadRequest) // Return a 400 error on a bad signature
  return
 }

 if event.Type == "checkout.session.completed" {
  var session stripe.CheckoutSession
  err := json.Unmarshal(event.Data.Raw, &session)
  if err != nil {
   log.Printf("Error parsing webhook JSON %s", err)
   w.WriteHeader(http.StatusBadRequest)
   return
  }

  params := &stripe.CheckoutSessionParams{}
  params.AddExpand("line_items")

  // Retrieve the session. If you require line items in the response, you may include them by expanding line_items.
  // Fulfill the purchase...

  
  var toFulfill User
  found := false
  for trial := 0; trial < 5; trial++ {
   if db.Where("checkout_session_id = ?", session.ID).First(&toFulfill).Error != nil {
    log.Println("Failed to fulfill user with ID " + session.ID)
   } else {
    found = true
    break
   }
  }
  if !found {
   log.Println("Error Failed to find user in database to fulfill: very bad! ID: " + session.ID)
  } else {
   userString, err := codeToString(toFulfill.Code)
   if err != nil {
    log.Printf("Error strange thing, saved user's code was unable to be converted to a string %s", err)
   }
   log.Printf("Fulfilling user with code %s\n", userString)
   toFulfill.IsFulfilled = true
   db.Save(&toFulfill)
  }
 }

 w.WriteHeader(http.StatusOK)
}

func checkout(w http.ResponseWriter, req *http.Request) {
 if doCors {
  w.Header().Set("Access-Control-Allow-Origin", "*")
 }

 // generate a code
 var newCode string
 var newCodeUser userCode
 found := false
 for i := 0; i < 1000; i++ {
  codeInt := rand.Intn(MaxCodes)
  newCodeUser = userCode(codeInt)
  var tmp User
  r := db.Where("Code = ?", newCodeUser).Limit(1).Find(&tmp)
  if r.RowsAffected == 0{
   var err error
   newCode, err = codeToString(newCodeUser)
   if err != nil {
    w.WriteHeader(http.StatusInternalServerError)
    log.Fatalf("Failed to generate code from random number: %s", err)
    return
   }
   found = true
   break
  }
 }
 if !found {
  w.WriteHeader(http.StatusInternalServerError)
  log.Fatal("Failed to find new code!!!")
  return
 }

 params := &stripe.CheckoutSessionParams {
  LineItems: []*stripe.CheckoutSessionLineItemParams {
   &stripe.CheckoutSessionLineItemParams{
    Price: stripe.String(daypassPriceId),
    Quantity: stripe.Int64(1),
   },
  },
  Mode: stripe.String(string(stripe.CheckoutSessionModePayment)),
  SuccessURL: stripe.String(checkoutRedirectTo),
  CancelURL: stripe.String(checkoutRedirectTo),
 }

 s, err := session.New(params)

 if err != nil {
  log.Printf("session.New: %v", err)
 }

 log.Printf("Creating user with checkout session ID %s\n", s.ID)
 result := db.Create(&User {
  Code: newCodeUser,
  BoughtTime: currentTime(),
  BoughtType: DayPass,
  IsFulfilled: false,
  CheckoutSessionID: s.ID,
 })
 if result.Error != nil {
  w.WriteHeader(http.StatusInternalServerError)
  log.Printf("Failed to write to database: %s", result.Error)
 } else {
  fmt.Fprintf(w, "%s|%s", newCode, s.URL)
 }
}

func index(w http.ResponseWriter, req *http.Request) {
 req.Body = http.MaxBytesReader(w, req.Body, 1024 * 1024) // no sending huge files to crash the server
 if doCors {
  w.Header().Set("Access-Control-Allow-Origin", "*")
 }
 bodyBytes, err := io.ReadAll(req.Body)
 if err != nil {
  w.WriteHeader(http.StatusBadRequest)
  log.Println("Bad error: ", err)
  return
 } else {
  bodyString := string(bodyBytes)
  splitBody := strings.Split(bodyString, "|")

  if len(splitBody) != 2 {
   w.WriteHeader(http.StatusBadRequest)
  }
  var promptString string = splitBody[1]
  var userToken string = splitBody[0]

  // see if need to pay
  rejected := false
  {
   if len(userToken) != 4 {
    log.Println("Rejected because not 4: `" + userToken + "`") 
    rejected = true
   } else {
    var thisUser User
    thisUserCode, err  := parseUserCode(userToken)
    if err != nil {
     log.Printf("Error: Failed to parse user token %s\n", userToken)
     rejected = true
    } else {
     if db.First(&thisUser, thisUserCode).Error != nil {
      log.Printf("User code %d string %s couldn't be found in the database: %s\n", thisUserCode, userToken, db.Error)
      rejected = true
     } else {
      if isUserOld(thisUser) {
       log.Println("User code " + userToken + " is old, not valid")
       db.Delete(&thisUser)
       rejected = true
      } else {
       rejected = false
      }
     }
    }
   }
  }
  if rejected {
   fmt.Fprintf(w, "0")
   return
  }

  if logResponses {
   log.Println("Println line prompt string: ", promptString)
  }

  ctx := context.Background()
  req := gogpt.CompletionRequest {
   Model:     "curie:ft-personal-2023-03-24-03-06-24",
   MaxTokens: 80,
   Prompt:    promptString,
   Temperature: 0.9,
   FrequencyPenalty: 0.0,
   PresencePenalty: 0.6,
   TopP: 1.0,
   Stop: []string{"\""},
   N: 1,
  }
  resp, err := c.CreateCompletion(ctx, req)
  if err != nil {
   log.Println("Error Failed to generate: ", err)
   w.WriteHeader(http.StatusInternalServerError)
   return
  }
  response := resp.Choices[0].Text
  if logResponses {
   log.Println("Println response: ", response)
  }
  fmt.Fprintf(w, "1%s", response)
 }
}

func currentTime() int64 {
 return time.Now().Unix()
}

func main() {
 var err error
 db, err = gorm.Open(sqlite.Open("rpgpt.db"), &gorm.Config{})
 if err != nil {
  log.Fatal(err)
 }
 db.AutoMigrate(&User{})

 clearOld(db)

 api_key := os.Getenv("OPENAI_API_KEY")
 if api_key == "" {
  log.Fatal("Must provide openai key")
 }
 checkoutRedirectTo = os.Getenv("REDIRECT_TO")
 if checkoutRedirectTo == "" {
  log.Fatal("Must provide a base URL (without slash) for playgpt to redirect to")
 }
 stripeKey := os.Getenv("STRIPE_KEY")
 if stripeKey == "" {
  log.Fatal("Must provide stripe key")
 }
 daypassPriceId = os.Getenv("PRICE_ID")
 if daypassPriceId == "" {
  log.Fatal("Must provide daypass price ID")
 }
 stripe.Key = stripeKey
 webhookSecret = os.Getenv("WEBHOOK_SECRET")
 if webhookSecret == "" {
  log.Fatal("Must provide webhook secret for receiving checkout completed events")
 }

 logResponses = os.Getenv("LOG_RESPONSES") != ""
 doCors = os.Getenv("CORS") != ""
 c = gogpt.NewClient(api_key)

 http.HandleFunc("/", index)
 http.HandleFunc("/webhook", webhookResponse)
 http.HandleFunc("/checkout", checkout)

 portString := ":8090"
 log.Println("Serving on " + portString + "...")
 http.ListenAndServe(portString, nil)
}

