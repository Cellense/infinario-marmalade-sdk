# Infinario Marmalade SDK

Infinario Marmalade SDK provides tracking capabilities for your application.

# Usage

To start tracking, you need to know your projectToken. To initialize the tracking, simply get an instance of the Infinario class:

```
std::string projectToken("my_project_token");
Infinario infinario(projectToken);
```

Now you can track events for an anonymous player (internally identified with a cookie). If you can identify the current player with a unique id (for instance an email address), you specify it when creating the instance:

```
std::string projectToken("my_project_token");
std::string customerId("player@example.com");
Infinario infinario(projectToken, customerId);
```

After creating an instance of the Infinario class you can start tracking events by specifying their name and attributes.

```
infinario.track("quest_started", "{ \"level\": \"11\", \"experience_points\": 1500 }");
infinario.track("quest_ended", "{ \"level\": \"13\", \"experience_points\": 2500, \"gold_earned\": 168 }");
```

##Identifying anonymous players

Once you call identify, the previously anonymous player is automatically merged with the newly identified player. Make sure to only call this once and only if the customerId was not given when the instance was created, as otherwise the behaviour is undefined.

```
infinario.identify("player@example.com");
```

If at any time you wish to track events for a different player, you need to create a new instance of the Infinario class, with a different customerId.

##Updating player attributes

Call the update method to set the current player's attributes:

```
infinario.update("{ \"level\": 1, \"kills\": 9001 }");
```

##Setting timestamps for tracked events

By default if no timestamp is specified the SDK uses the time when the track method was called as the timestamp. You could however specify your own timestamp, the timestamp is a double value where the whole part is the number of seconds from 01-01-1970 (standard Unix Timestamp) and the decimal part specifies milliseconds.

```
infinario.track("death", "{}", 1149573966.000); // Equal to 06-06-2006 06:06:06
```

##Callbacks

Each of the commands (identify, update and track), has two optional parameters: a callback function pointer and a pointer to custom data that are passed to the callback function when it is called. The callback function is called after a reply is successfully recieved from the server or if an error occurs.

```
void callbackFoo(const CIwHTTP &, const Infinario::ResponseStatus &responseStatus,
    const std::string &responseBody, void *)
{
    if (responseStatus == Infinario::ResponseStatus::Success) {
        // Prints the recieved JSON response from the Infinario server, if all went well.
        std::cout << responseBody << std::endl;
    } else {
        std::cout << "An error occurred" << std::endl;
    }
}

void callbackBar(const CIwHTTP &, const Infinario::ResponseStatus &,
    const std::string &, void *userData)
{
    std::string *stringData = reinterpret_cast<void *>(userData);
    std::cout << stringData << std::endl;
    delete stringData;
}

.
.
.

infinario.track("respawn", "{}", callbackFoo); // No custom data is sent.

std::string *stringData = new std::string("Hello there!");
infinario.update("respawn", "{}", callbackBar, reinterpret_cast<void *>(data));
```

When sending custom data to the callback function, ensure that the pointer is valid (ie. the underlying
memory does not get deallocated) until the response arrives from the server.

##Additional notes

You can run as many instances of the Infinario class as you please. The current implementation is not thread safe and
so instances of the Infinario class should not be shared between threads (each thread should exclusively use its own
instances).

On a single Infinatio class instance the commands are chained to be executed one after another (there is never more than one
command being executed at any given time). The infinario object maintains an internal queue of all schedueled commands and thus
they are always executed precisely in the same order, in which they were called.
