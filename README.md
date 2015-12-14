# Infinario Marmalade SDK

The SDK enables you to easily use Infinario's powerful tracking capabilities in your Marmalade application.

Make sure to checkout the `InfinarioSDK.mkb` Marmalade project file. When loaded by Marmalade Hub it can be easily used to test the SDK's functionality. Also you should take a short look at the file `Test.cpp` as it contains some useful examples of how to use the SDK. More information on how to run the test project can be found at the bottom of this readme.

##Quick Start

To start tracking all you need to do is to add the contents of the `src` folder into your project, and include the header file `Infinario.h` anywhere you wish to track player events.

You will need to have an instance of the Infinario class. Due to the way Marmalade manages memory, make sure to create this instance after calling the standard initialization functions (like `IwGxInit();`) and destroy it before calling the standard termination function (like `IwGxTerminate();`).

Here's an example:

```
// The unique projectToken generated by the Infinario Server for your project.
const std::string projectToken("my_project_token");

// A unique identifier for the currently tracked player (in this case an e-mail address).
const std::string customerId("infinario@example.com");

// Create an instance of the Infinario class.
Infinario::Infinario infinario(projectToken, customerId);

.
.
.

// Track the event "player_jumped" with no additional properties.
infinario.track("player_jumped", "{}");

.
.
.

// Track the event "player_died" with the properties argument specifying the place.
infinario.track("player_died", "{ \"location\": \"Green Hill\" }");
```

The first argument of the `track()` method is the tracked event's name, the second are the tracked event's attributes. The attributes may contain anything, but must be a valid JSON string.

##How it works

After the `track()` method is called, the Infinario class internally schedules a request to be sent asynchronously to the Infinario server. This means that code execution continues immediately and does not wait for the response to return, but rather processes it in the background once it arrives. This is how the Infinario class handles all methods that need to communicate with the Infinario server (there are two more such methods, namely the `identify()` and `update()` methods, which will be described later).

Each instance of the Infinario class maintains an internal queue of pending requests. Whenever one of the methods `track()`, `identify()` or `update()` is called, the request is added to the end of the queue. The Infinario SDK processes requests, one at a time, so a new request is sent only when the last one has been finalized (meaning we either got a response or the request failed for some reason) or when there are no requests waiting to be sent.

By default, responses from the server are handled internally within the Infinario class. We will later discuss a way to handle responses using user-defined callback functions.

##Anonymous Player

In some cases we cannot uniquely identify the player, and we need to use a temporary identifier. The Infinario class can automatically generate what is called a `customerCookie`, which will be used instead of the `customerId` to track events until we are able to identify the player.

To track events for an anonymous player we only supply the first argument to the Infinario class's constructor:

```
// The unique projectToken generated by the Infinario Server for your project.
const std::string projectToken("my_project_token");

// Create an instance of the Infinario class, which internally generates a
// 'customerCookie' to identify the anonymous player.
// The 'customerCookie' is a random string generated using the device's hardware
// and running OS information.
Infinario::Infinario infinario(projectToken);

.
.
.

// An event tracked for the anonymous player.
infinario.track("quest_started", "{ \"level\": \"11\", \"experience_points\": 1500 }");
```

If at any time we wish to add a unique `customerId` to our anonymous player or merge all the data we have tracked so far with an existing player, we can use the `identify()` method:

```
// Now all the data tracked so far will be assigned to the player with the unique id 'infinario@example.com'
infinario.identify("infinario@example.com");
```

The only way we could know that the identify method succeeded is when we receive a confirmation from the Infinario server. That is why the `customerId` we gave to the `identify()` method will only be assigned to events that are tracked after we received the Infinario server's response. Don't worry, all requests sent between calling the `identify()` method and us receiving the Infinario server's confirmation are handled correctly.

It is strongly recommended that you call the `identify()` method at most once and only on instances of the Infinario class that were constructed without a `customerId`. The `identify()` method cannot be used to change an existing player's `customerId`.

##Updating player attributes

In addition to tracking a player’s events we can store static information in the form of attributes for any given player. Call the `update()` method to set the current player's attributes:

```
infinario.update("{ \"level\": 1, \"kills\": 9001 }");
```

The first argument are the player's new attributes which will be merged with any existing attributes. The attributes may contain anything, but must be a valid JSON string.

##Setting timestamps for tracked events

By default, if no timestamp is specified the SDK uses the time when the `track()` method was called as the event's timestamp. You could however specify your own timestamp, the timestamp is a double value where the whole part is the number of seconds passed since 01-Jan-1970 (standard Unix Timestamp) and the decimal part specifies milliseconds.

```
infinario.track("death", "{}", 1149573966.000); // Equal to 06-06-2006 06:06:06
```

##Callbacks

Since requests are proccessed asynchronously, user-defined callback functions provide a way to react to responses from the Infinario server.

A general note of caustion. When passing custom data to these functions or when using external data within them, make sure that the variables remain valid (ie. the underlying memory does not get deallocated), so it can be used when the callback function gets called.

###Response Callbacks

Each of the methods that need to communicate with the Infinario server (`identify()`, `update()` and `track()`), has two optional parameters: a callback function pointer and a pointer to custom data that will be passed to the callback function when it is called. The callback function is called after a response is successfully received from the server or if an error occurs while processing the request.

```
// This is the definition of a callback function.
void CallbackFoo(const CIwHTTP &, const std::string &requestBody, const Infinario::ResponseStatus &responseStatus,
    const std::string &responseBody, void *)
{
    // First we check if the response was successfully recieved.
    if (responseStatus == Infinario::ResponseStatus::Success) {
        // Prints the received JSON response from the Infinario server.
        std::cout << responseBody << std::endl;
    } else {
        std::cout << "An error occurred" << std::endl;
    }
}

// And here's another definition of a callback function.
void CallbackBar(const CIwHTTP &, const std::string &requestBody, const Infinario::ResponseStatus &,
    const std::string &, void *userData)
{
    std::string *stringData = reinterpret_cast<void *>(userData);
    std::cout << stringData << std::endl; // Prints out "Hello there!".
    delete stringData;
}

.
.
.

// No custom data is sent to this callback.
infinario.track("respawn", "{}", callbackFoo);

// A string is sent to be available to the callback when the response arrives.
std::string *stringData = new std::string("Hello there!");
infinario.update("respawn", "{}", callbackBar, reinterpret_cast<void *>(data));
```

You can see that within the `ResponseCallback` functions we are given 5 arguments:
* `httpClient` - a reference to the object used internally by the Infinario class to send requests. Detailed information about the currently processed request can be obtained by querying this object. This is useful when debugging.
* `requestBody` - the full HTTP request body sent by the Infinario SDK to the Infinario server. This is useful when debugging.
* `responseStatus` - this indicates whether the request was completed successfully or failed due to an error. The enum variable can have one of 5 values, each describing a different situation:
   * `Infinario::ResponseStatus::Success` - the request was sent and a response was successfully received.
   * `Infinario::ResponseStatus::SendRequestError` - the request wasn't sent.
   * `Infinario::ResponseStatus::ReceiveHeaderError` - the request was sent, but no response was recieved or an error occured while loading the recieved data.
   * `Infinario::ResponseStatus::RecieveBodyError` - the request was sent and a response was received, but an error occured when loading the received data.
   * `Infinario::ResponseStatus::KilledError` - the Infinario class instance was destroyed before the request can be finalized. In some cases the request could have already been sent to the Infinario server.
* `responseBody` - the full HTTP response body received from the Infinario server. This can be used to check if the server correctly processed the sent request.
* `userData` - a pointer to the custom data supplied to the method where response callback was assigned (in our case the method `update()`).

###Empty Request Queue Callbacks

In the previous callback, the last value of the `responseStatus` parameter highlights a useful feature of the Infinario SDK. In some cases, the Infinario class instance may be destroyed before all requests in the queue are processed. If this happens the callback functions for all the remaining requests are called within the Infinario class's destructor.

However, a request within this queue could have already been sent and in that case we will have no way of finding out the server's response. We can avoid this situation by using the Empty Request Queue callback, which is called when the last request in the queue has been processed. Within it we can safely destroy the Infinario class instance.

There probably are other uses for this callback, this is an an implementation of the usecase described above:

```
// This is the definition of a callback function.
void SelfDestructorCallback(void *userData)
{
    Infinario::Infinario *infinario = reinterpret_cast<Infinario::Infinario *>(userData);
    
    if (infinario != NULL) {
        // This callback will get called in the destructor, so we need to remove it first.
        infinario->ClearEmptyRequestQueueCallback();
        delete infinario;
    }
}

.
.
.

Infinario::Infinario *infinario = new Infinario::Infinario();
// We send the Infinario class instance itself to the callback.
infinario->SetEmptyRequestQueueCallback(SelfDestructorCallback, reinterpret_cast<void *>(infinario));
```

As you can see the `EmptyRequestQueueCallback` function has only one argument and that's the data we gave it when we assigned the callback by calling the `SetEmptyRequestQueueCallback()` method.

##Using a proxy

We can route requests through a proxy server like this:

```
infinario.SetProxy("127.0.0.1:8888");
```

From now on all requests will be sent through the given proxy server. To send requests directly again just call:

```
infinario.ClearProxy();
```

##Test Project

The file `InfinarioSDK.mkb` can be used to setup a Marmalade project, all you need to do is open it in the Marmalade Hub application. Before running the project make sure to find and correctly setup the following preprocessor definitions in `Test.cpp`:
* `TEST_PROJECT_TOKEN` - here you should enter your `projectToken`, which was generated for your project by the Infinario server.
* `TEST_CUSTOMER_ID` - all the tests will run for a single player with the given `customerId`. You can then easily find this player and remove him from your project.

If everything is setup correctly you should be able to run the project and all tests will be colored green.

##Additional Notes

For the most accurate information (exact method prototypes and some helpful information on how to use the SDK's methods) be sure to take a look at the file `src/Infinario.h`.

You can run as many instances of the Infinario class as you wish, though for most cases one instance will be sufficient.

The current implementation is thread safe and so an instance of the Infinario class may be shared by multiple threads.

Tested on Marmalade v8.0.0.
