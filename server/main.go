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
 "github.com/stripe/stripe-go/v74/event"
 "github.com/stripe/stripe-go/v74/webhook"
 "github.com/stripe/stripe-go/v74/checkout/session"

 "gorm.io/gorm"
 "gorm.io/driver/sqlite"

 "github.com/creikey/rpgpt/server/codes"
)

// BoughtType values. do not reorganize these or you fuck up the database
const (
 DayPass = iota
)

const (
 // A-Z and 0-9, four digits means this many codes
 MaxCodes = 36 * 36 * 36 * 36
)


type User struct {
 CreatedAt time.Time
 UpdatedAt time.Time
 DeletedAt gorm.DeletedAt `gorm:"index"`

 Code  codes.UserCode `gorm:"primaryKey"` // of maximum value max codes, incremented one by one. These are converted to 4 digit alphanumeric code users can remember/use
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
var daypassTimedOut = make(map[codes.UserCode]int64) // value is time last requested, rate limiting by day pass. If exists is rate limited, should be removed when ok to request again
// for 10 free minutes a day, is when ip address began requesting
var ipAddyTenFree = make(map[string]int64)

func cleanTimedOut() {
 for k, v := range daypassTimedOut {
  if currentTime() - v > 1 {
   delete(daypassTimedOut, k)
  }
 }
 for k, v := range ipAddyTenFree {
  if currentTime() - v > 24*60*60 {
   delete(ipAddyTenFree, k)
  }
 }
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
 var toDelete []codes.UserCode // codes
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

func handleEvent(event stripe.Event) error {
 if event.Type == "checkout.session.completed" {
  var session stripe.CheckoutSession
  err := json.Unmarshal(event.Data.Raw, &session)
  if err != nil {
   return fmt.Errorf("Error parsing webhook JSON %s", err)
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
   return fmt.Errorf("Error Failed to find user in database to fulfill: very bad! ID: " + session.ID)
  } else {
   userString, err := codes.CodeToString(toFulfill.Code)
   if err != nil {
    return fmt.Errorf("Error strange thing, saved user's code was unable to be converted to a string %s", err)
   }
   log.Printf("Fulfilling user with code %s number %d\n", userString, toFulfill.Code)
   if(toFulfill.IsFulfilled) {
    log.Printf("User with code %s is already fulfilled, strange\n", userString)
   }
   toFulfill.IsFulfilled = true
   err = db.Save(&toFulfill).Error
   if err != nil {
    return fmt.Errorf("Failed to save fulfilled flag status to database: %s", err)
   }
  }
 }
 return nil
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

 err = handleEvent(event)
 if err != nil {
  w.WriteHeader(http.StatusBadRequest)
 }
 
 w.WriteHeader(http.StatusOK)
}

func checkout(w http.ResponseWriter, req *http.Request) {
 if doCors {
  w.Header().Set("Access-Control-Allow-Origin", "*")
 }

 // generate a code
 var newCode string
 var newCodeUser codes.UserCode
 found := false
 for i := 0; i < 1000; i++ {
  codeInt := rand.Intn(MaxCodes)
  newCodeUser = codes.UserCode(codeInt)
  var tmp User
  r := db.Where("Code = ?", codeInt).Limit(1).Find(&tmp)
  if r.RowsAffected == 0{
   var err error
   newCode, err = codes.CodeToString(newCodeUser)
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

 customMessage := fmt.Sprintf("**IMPORTANT** Your Day Pass Code is %s", newCode)
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
  CustomText: &stripe.CheckoutSessionCustomTextParams{ Submit: &stripe.CheckoutSessionCustomTextSubmitParams { Message: &customMessage } },
 }

 s, err := session.New(params)

 if err != nil {
  log.Printf("session.New: %v", err)
 }

 log.Printf("Creating user code %s code integer %d with checkout session ID %s\n", newCode, newCodeUser ,s.ID)
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
  log.Printf("SUccessfully created usercode!\n")
  fmt.Fprintf(w, "%s|%s", newCode, s.URL)
 }
}

func completion(w http.ResponseWriter, req *http.Request) {
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
   log.Println("Weird body length %d not 2\n", len(splitBody))
   return
  }
  var promptString string = splitBody[1]
  var userToken string = splitBody[0]

  // see if need to pay
  rejected := false
  cleanTimedOut()
  {
   if len(userToken) != 4 {
    // where I do the IP rate limiting

    userKey := req.RemoteAddr
    createdTime, ok := ipAddyTenFree[userKey]
    if !ok {
     ipAddyTenFree[userKey] = currentTime()
     rejected = false
    } else {
     if currentTime() - createdTime < 10*60 {
      rejected = false
     } else {
      rejected = true // out of free time buddy
     }
    }
   } else {
    var thisUser User
    thisUserCode, err := codes.ParseUserCode(userToken)
    if err != nil {
     log.Printf("Error: Failed to parse user token %s\n", userToken)
     rejected = true
    } else {
     err := db.First(&thisUser, thisUserCode).Error 
     if err != nil {
      log.Printf("User code %d string %s couldn't be found in the database: %s\n", thisUserCode, userToken, err)
      rejected = true
     } else {
      if isUserOld(thisUser) {
       log.Println("User code " + userToken + " is old, not valid")
       db.Delete(&thisUser)
       rejected = true
      } else {
       // now have valid user, in the database, to be rate limit checked
       // rate limiting based on user token
       if !thisUser.IsFulfilled {
        log.Println("Unfulfilled user trying to play, might've been unresponded to event. Retrieving backlog of unfulfilled events...\n")
        params := &stripe.EventListParams{}
        params.Filters.AddFilter("delivery_success", "", "false")
        i := event.List(params)
        for i.Next() {
         e := i.Event()
         log.Println("Unfulfilled event! Of type %s. Handling...\n", e.Type)
         err := handleEvent(*e)
         if err != nil {
          log.Println("Failed to fulfill unfulfilled event: %s\n", err)
         }
        }
       }
       if thisUser.IsFulfilled {
        _, exists := daypassTimedOut[thisUserCode]
        if exists {
         rejected = true
        } else {
         rejected = false
         daypassTimedOut[thisUserCode] = currentTime()
        }
       } else {
        log.Println("User with code and existing entry in database was not fulfilled, and wanted to play... Very bad. Usercode: %s\n", thisUserCode)
       }
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
   Model:     "curie:ft-alnar-games-2023-04-01-21-23-38",
   MaxTokens: 80,
   Prompt:    promptString,
   Temperature: 0.9,
   FrequencyPenalty: 0.0,
   PresencePenalty: 0.6,
   TopP: 1.0,
   Stop: []string{"\n"},
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
   log.Println("Println response: `", response + "`")
   log.Println()
  }
  fmt.Fprintf(w, "1%s", response)
 }
}

func currentTime() int64 {
 return time.Now().Unix()
}

func main() {
 rand.Seed(time.Now().UnixNano())
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

 http.HandleFunc("/completion", completion)
 http.HandleFunc("/webhook", webhookResponse)
 http.HandleFunc("/checkout", checkout)

 portString := ":8090"
 log.Println("DO NOT RUN WITH CLOUDFLARE PROXY it rate limits based on IP, if behind proxy every IP will be the same. Would need to fix req.RemoteAddr. Serving on " + portString + "...")
 http.ListenAndServe(portString, nil)
}

