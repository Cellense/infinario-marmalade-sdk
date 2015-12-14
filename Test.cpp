#include "../src/Infinario.h"
#include "Test.h"

#include "IwGx.h"

#include "s3e.h"
#include "s3eFile.h"

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

Test::~Test()
{}

bool Test::IsComplete() const
{
	Test::State state = this->GetState();
	return (state == State::Succeeded) || (state == State::Failed);
}

void Test::Render(const int32 x, const int32 y, const int32 id) const
{
	std::string color;
	std::string message;

	switch (this->GetState())
	{
	case State::Running:
		color = "`xff0000";
		message = "Running";
		break;
	case State::Failed:
		color = "`x0000ff";
		message = "Failed";
		break;
	case State::Succeeded:
		color = "`x00ff00";
		message = "Succeeded";
		break;
	default:
		color = "`x000000";
		break;
	}

	std::stringstream sstream;
	sstream << color << "Test " << id << ": " << message;

	IwGxPrintString(x, y, sstream.str().c_str(), false);
}

void Test::OutputLog(const int32 id) const
{
	std::stringstream outputFilenameStream;
	outputFilenameStream << "test_" << id << ".log";

	s3eFile* outputFile = s3eFileOpen(outputFilenameStream.str().c_str(), "w");

	std::string outputString(this->log.str());
	s3eFileWrite(reinterpret_cast<const void *>(outputString.c_str()), outputString.size(), 1, outputFile);

	s3eFileClose(outputFile);
}

// Test Parameters.

#define TEST_PROJECT_TOKEN "my_project_token"
#define TEST_CUSTOMER_ID "infinario@example.com"

const std::string projectToken(TEST_PROJECT_TOKEN);
const std::string customerId(TEST_CUSTOMER_ID);

// Callback functions.

typedef struct TestEmptyRequestQueueUserData
{
	std::vector<bool>::iterator isCalled;
	Infinario::Infinario *infinario;
	std::string *message;
	std::stringstream *log;
} TestEmptyRequestQueueUserData;

void TestEmptyRequestQueueCallback(void *userData)
{
	TestEmptyRequestQueueUserData *data = reinterpret_cast<TestEmptyRequestQueueUserData *>(userData);
	
	*(data->log) << "EmptyRequestQueueCallback {" << std::endl;
	
	*(data->isCalled) = 1;
	*(data->log) << "--isCalled flag set--" << std::endl << "True" << std::endl;
	
	if (data->infinario != NULL) {
		data->infinario->ClearEmptyRequestQueueCallback(); // So we don't call ourselves in the next instruction.
		delete data->infinario;
		*(data->log) << "--Infinario instance destroyed--" << std::endl;
	}

	if (data->message != NULL) {
		*(data->log) << "--UserData Message--" << std::endl << *(data->message) << std::endl;
		delete data->message;
	}

	*(data->log) << "}" << std::endl;

	delete data;
}

typedef struct TestResponseUserData
{
	std::vector<bool>::iterator isCalled;
	std::vector<bool>::iterator isSuccessfull;
	std::string *message;
	std::stringstream *log;
} TestResponseUserData;

void TestResponseCallback(const CIwHTTP *httpClient, const std::string &requestBody,
	const Infinario::ResponseStatus responseStatus, const std::string &responseBody, void *userData)
{
	TestResponseUserData *data = reinterpret_cast<TestResponseUserData *>(userData);
	
	*(data->log) << "ResponseCallback {" << std::endl;
	
	*(data->isCalled) = true;
	*(data->log) << "--isCalled flag set--" << std::endl << "True" << std::endl;
	
	*(data->isSuccessfull) = (responseStatus == Infinario::ResponseStatus::Success) &&
		(responseBody.find("\"status\": \"ok\"") != std::string::npos);
	*(data->log) << "--isSuccessfull flag set--" << std::endl;
	*(data->log) << (*(data->isSuccessfull) ? "True" : "False") << std::endl;
	
	*(data->log) << "--Original Request--" << std::endl << requestBody << std::endl;
	
	*(data->log) << "--Response Status--" << std::endl;
	switch (responseStatus)
	{
	case Infinario::ResponseStatus::Success:
		*(data->log) << "Success";
		break;
	case Infinario::ResponseStatus::SendRequestError:
		*(data->log) << "SendRequestError";
		break;
	case Infinario::ResponseStatus::ReceiveHeaderError:
		*(data->log) << "ReceiveHeaderError";
		break;
	case Infinario::ResponseStatus::RecieveBodyError:
		*(data->log) << "RecieveBodyError";
		break;
	case Infinario::ResponseStatus::KilledError:
		*(data->log) << "KilledError";
		break;
	default:
		*(data->log) << "UnknownStatus";
		break;
	}
	*(data->log) << std::endl;

	*(data->log) << "--Response Body--" << std::endl << responseBody << std::endl;
	
	if (data->message != NULL) {
		*(data->log) << "--UserData Message--" << std::endl << *(data->message) << std::endl;
		delete data->message;
	}
	
	*(data->log) << "}" << std::endl;
	
	delete data;
}

class CallbackTest : public Test
{
protected:
	TestEmptyRequestQueueUserData *CreateTestEmptyRequestQueueUserData()
	{
		this->_callbackFlags.push_back(false);

		TestEmptyRequestQueueUserData *result = new TestEmptyRequestQueueUserData();

		result->isCalled = (this->_callbackFlags.end() - 1);
		result->infinario = NULL;
		result->message = NULL;
		result->log = &(this->log);

		return result;
	}

	TestResponseUserData *CreateTestResponseUserData()
	{
		this->_callbackFlags.push_back(false);
		this->_successFlags.push_back(false);

		TestResponseUserData *result = new TestResponseUserData();
		
		result->isCalled = (this->_callbackFlags.end() - 1);
		result->isSuccessfull = (this->_successFlags.end() - 1);
		result->message = NULL;
		result->log = &(this->log);

		return result;
	}

	bool TestCallbackFlags() const
	{
		for (std::vector<bool>::size_type i = 0, count = this->_callbackFlags.size(); i < count; ++i) {
			if (!(this->_callbackFlags[i])) {
				return false;
			}
		}
		return true;
	}

	bool TestSuccessFlags() const
	{
		for (std::vector<char>::size_type i = 0, count = this->_successFlags.size(); i < count; ++i) {
			if (!(this->_successFlags[i])) {
				return false;
			}
		}
		return true;
	}
	
	virtual State GetState() const
	{
		if (this->TestCallbackFlags()) {
			if (this->TestSuccessFlags()) {
				return State::Succeeded;
			} else {
				return State::Failed;
			}
		} else {
			return State::Running;
		}		
	}

	std::vector<bool> _callbackFlags;
	std::vector<bool> _successFlags;
};

// Actual tests.

class Test1 : public CallbackTest
{
public:
	virtual void Init()
	{
		// Test tracking using constructor with explicit customer identifier.
		this->_infinario = new Infinario::Infinario(projectToken, customerId);
		
		// Testing setting of customer attributes.
		this->_infinario->Update("{ \"name\": \"Rumbal\", \"age\": 12, \"e-peen\": 1.2364 }",
			TestResponseCallback, reinterpret_cast<void *>(this->CreateTestResponseUserData()));
		
		// Testing basic event tracking.
		this->_infinario->Track("omg", "{ \"quest\": \"drag\", \"loot\" : \"zidan\", "
			"\"rly?\" : 52, \"mesi\" : 7.41 }", 1449008100.0,
			TestResponseCallback, reinterpret_cast<void *>(this->CreateTestResponseUserData()));
	}

	virtual void Update()
	{}

	virtual void Terminate()
	{
		delete this->_infinario;
	}
private:
	Infinario::Infinario *_infinario;
};

class Test2 : public CallbackTest
{
public:
	virtual void Init()
	{
		// Test tracking using constructor with anonymous customer that is identified before the event.
		this->_infinario = new Infinario::Infinario(projectToken);

		// Tests customer merging.
		this->_infinario->Identify(customerId,
			TestResponseCallback, reinterpret_cast<void *>(this->CreateTestResponseUserData()));

		// Testing basic event tracking.
		this->_infinario->Track("omg", "{ \"quest\": \"balz\", \"loot\" : \"uwomat?\", \"rly?\" : 4, \"mesi\" : 2.1 }",
			1449008256.0, TestResponseCallback, reinterpret_cast<void *>(this->CreateTestResponseUserData()));
	}

	virtual void Update()
	{}

	virtual void Terminate()
	{
		delete this->_infinario;
	}
private:
	Infinario::Infinario *_infinario;
};

class Test3 : public CallbackTest
{
public:
	static void CustomResponseCallback(const CIwHTTP *httpClient, const std::string &requestBody,
		const Infinario::ResponseStatus responseStatus, const std::string &responseBody, void *userData)
	{
		*(reinterpret_cast<bool *>(userData)) = true;
	}

	typedef struct CustomEmptyRequestQueueUserData
	{
		CustomEmptyRequestQueueUserData **emptyRequestQueueUserData;
		TestEmptyRequestQueueUserData *userData;
	} CustomEmptyRequestQueueUserData;

	static void CustomEmptyRequestQueueCallback(void *userData)
	{
		CustomEmptyRequestQueueUserData *data = reinterpret_cast<CustomEmptyRequestQueueUserData *>(userData);
		
		TestEmptyRequestQueueCallback(data->userData);

		(*data->emptyRequestQueueUserData) = NULL;

		delete data;
	}

	virtual void Init()
	{
		this->_enablePrint = false;

		// Test tracking using constructor with anonymous customer that is identified after the event.
		this->_infinario = new Infinario::Infinario(projectToken);

		// Set a self destruction callback when all requests are finished.
		this->_emptyRequestQueueUserData = new CustomEmptyRequestQueueUserData();
		this->_emptyRequestQueueUserData->emptyRequestQueueUserData = &(this->_emptyRequestQueueUserData);
		this->_emptyRequestQueueUserData->userData = this->CreateTestEmptyRequestQueueUserData();
		this->_emptyRequestQueueUserData->userData->infinario = this->_infinario;
		this->_infinario->SetEmptyRequestQueueCallback(
			CustomEmptyRequestQueueCallback, reinterpret_cast<void *>(this->_emptyRequestQueueUserData));

		this->_infinario->Track("omg", "{ \"quest\": \"ballzianus\", \"loot\" : \"herrba\", \"rly?\" : 4112, \"mesi\""
			" : 211.41 }", 1449008523.0,
			TestResponseCallback, reinterpret_cast<void *>(this->CreateTestResponseUserData()));
		this->_infinario->Identify(customerId,
			TestResponseCallback, reinterpret_cast<void *>(this->CreateTestResponseUserData()));

		// Testing update of customer attributes.
		this->_infinario->Update("{ \"name\": \"NotSoRUmb\", \"noob\" : false, \"e-peen\": 1000.2364 }",
			TestResponseCallback, reinterpret_cast<void *>(this->CreateTestResponseUserData()));

		// Testing long and short tracking bodies.
		this->_infinario->Track("bigone", "{ \"name\": \"Get\", \"name2\" : \"Ready\", \"name3\" : \"To\","
			" \"oh_oh\" : false, \"blaster\" : \"At vero eos et accusamus et iusto odio dignissimos ducimus qui"
			" blanditiis praesentium voluptatum deleniti atque corrupti quos dolores et quas molestias excepturi"
			" sint occaecati cupiditate non provident, similique sunt in culpa qui officia deserunt mollitia animi,"
			" id est laborum et dolorum fuga.Et harum quidem rerum facilis est et expedita distinctio.Nam libero"
			" tempore, cum soluta nobis est eligendi optio cumque nihil impedit quo minus id quod maxime placeat"
			" facere possimus, omnis voluptas assumenda est, omnis dolor repellendus.Temporibus autem quibusdam et"
			" aut officiis debitis aut rerum necessitatibus saepe eveniet ut et voluptates repudiandae sint et"
			" molestiae non recusandae.Itaque earum rerum hic tenetur a sapiente delectus, ut aut reiciendis"
			" voluptatibus maiores alias consequatur aut perferendis doloribus asperiores repellat.On the other hand,"
			" we denounce with righteous indignation and dislike men who are so beguiled and demoralized by the charms"
			" of pleasure of the moment, so blinded by desire, that they cannot foresee the pain and trouble that are"
			" bound to ensue; and equal blame belongs to those who fail in their duty through weakness of will, which"
			" is the same as saying through shrinking from toil and pain.These cases are perfectly simple and easy to"
			" distinguish.In a free hour, when our power of choice is untrammelled and when nothing prevents our being"
			" able to do what we like best, every pleasure is to be welcomed and every pain avoided.But in certain"
			" circumstances and owing to the claims of duty or the obligations of business it will frequently occur"
			" that pleasures have to be repudiated and annoyances accepted.The wise man therefore always holds in"
			" these matters to this principle of selection : he rejects pleasures to secure other greater pleasures,"
			" or else he endures pains to avoid worse pains.\" }", 1449008622.0,
			TestResponseCallback, reinterpret_cast<void *>(this->CreateTestResponseUserData()));
		this->_infinario->Track("lost", "{ \"huh\": \"0.o\", \"orly\" : \"yrly\" }", 1449008822.0,
			TestResponseCallback, reinterpret_cast<void *>(this->CreateTestResponseUserData()));

		// Test tracking with no callback.
		this->_infinario->Track("cram", "{ \"hur\": \"dur\" }", 1449010822.0);

		// Test tracking with callback data.
		TestResponseUserData *responseUserData = this->CreateTestResponseUserData();
		responseUserData->message = new std::string("pika pika");
		this->_infinario->Track("zam", "{ \"mur\": \"xc\" }", 1449010822.0,
			TestResponseCallback, reinterpret_cast<void *>(responseUserData));

		// Test tracking without timestamp.
		this->_infinario->Track("flam", "{ \"hzm\": \"qw\" }", Test3::CustomResponseCallback, &(this->_enablePrint));
		this->_infinario->Track("ham", "{ \"food\": \"mickeyD's\" }");
	}

	virtual void Update()
	{
		if (this->_enablePrint) {
			IwGxPrintString(30, 10, "`x444444""InfinarioSDK for Marmalade Test.", false);
		}
	}

	virtual void Terminate()
	{
		if (this->_emptyRequestQueueUserData != NULL) {
			this->_infinario->ClearEmptyRequestQueueCallback();

			delete this->_emptyRequestQueueUserData->userData;

			delete this->_emptyRequestQueueUserData;
			this->_emptyRequestQueueUserData = NULL;

			delete this->_infinario;
		}
	}
private:
	Infinario::Infinario *_infinario;

	bool _enablePrint;

	CustomEmptyRequestQueueUserData *_emptyRequestQueueUserData;
};

class Test4 : public CallbackTest
{
public:
	typedef struct CustomEmptyRequestQueueUserData
	{
		bool *initUpdate;
		TestEmptyRequestQueueUserData *userData;
	} CustomEmptyRequestQueueUserData;

	static void CustomEmptyRequestQueueCallback(void *userData)
	{
		CustomEmptyRequestQueueUserData *data = reinterpret_cast<CustomEmptyRequestQueueUserData *>(userData);
		(*data->initUpdate) = true;

		TestEmptyRequestQueueCallback(data->userData);

		delete data;
	}

	virtual void Init()
	{
		this->_initUpdate = false;

		// Testing two independant subsequent chains of execution.
		this->_infinario = new Infinario::Infinario(projectToken);
		
		CustomEmptyRequestQueueUserData *emptyRequestQueueUserData = new CustomEmptyRequestQueueUserData();
		emptyRequestQueueUserData->initUpdate = &(this->_initUpdate);
		emptyRequestQueueUserData->userData = this->CreateTestEmptyRequestQueueUserData();
		this->_infinario->SetEmptyRequestQueueCallback(
			Test4::CustomEmptyRequestQueueCallback, emptyRequestQueueUserData);

		this->_responseUserData = this->CreateTestResponseUserData();
		this->_responseUserData->message = new std::string("1");
		this->_infinario->Track("omg", "{ \"quest\": \"ballzianus\", \"loot\" : \"herpaderba\","
			" \"rly?\" : 4112, \"messi\" : 211.41 }", 1449008523.0,
			TestResponseCallback, reinterpret_cast<void *>(this->_responseUserData));

		this->_responseUserData = this->CreateTestResponseUserData();
		this->_responseUserData->message = new std::string("2");
		this->_infinario->Identify(customerId,
			TestResponseCallback, reinterpret_cast<void *>(this->_responseUserData));

		this->_responseUserData = this->CreateTestResponseUserData();
		this->_responseUserData->message = new std::string("3");
		this->_infinario->Track("omg", "{ \"quest\": \"ballz\", \"loot\" : \"uwotmate?\", "
			"\"rly?\" : 42, \"messi\" : 2.41 }", 1449008256.0,
			TestResponseCallback, reinterpret_cast<void *>(this->_responseUserData));

		this->_responseUserData = this->CreateTestResponseUserData();
		this->_responseUserData->message = new std::string("4");
		this->_infinario->Update("{ \"name\": \"NotSoRUmb\", \"noob\" : false, \"e-peen\": 1000.2364 }",
			TestResponseCallback, reinterpret_cast<void *>(this->_responseUserData));

		this->_responseUserData = NULL;
	}

	virtual void Update()
	{
		if (this->_initUpdate) {
			this->_initUpdate = false;

			this->_infinario->ClearEmptyRequestQueueCallback();

			// Testing order of command execution Part2.
			this->_responseUserData = this->CreateTestResponseUserData();
			this->_responseUserData->message = new std::string("5");
			this->_infinario->Update("{ \"name\": \"Rumbal\", \"age\": 12, \"e-peen\": 1.2364 }",
				TestResponseCallback, reinterpret_cast<void *>(this->_responseUserData));

			this->_responseUserData = this->CreateTestResponseUserData();
			this->_responseUserData->message = new std::string("6");
			this->_infinario->Track("omg", "{ \"quest\": \"dragon\", \"loot\" : \"zidane\", "
				"\"rly?\" : 52, \"messi\" : 7.41 }", 1449008100.0,
				TestResponseCallback, reinterpret_cast<void *>(this->_responseUserData));

			this->_responseUserData = NULL;
		}
	}

	virtual void Terminate()
	{
		delete this->_infinario;
	}
private:
	Infinario::Infinario *_infinario;

	TestResponseUserData *_responseUserData;;
	bool _initUpdate;
};

class Test5 : public CallbackTest
{
public:
	virtual void Init()
	{
		// Test fast destruction of infinario object and allocation on stack instead of heap;
		Infinario::Infinario infinario(projectToken, customerId);
		
		TestResponseUserData *responseUserData = this->CreateTestResponseUserData();
		responseUserData->message = new std::string("Why so fast?");
		infinario.Track("gg", "{}", TestResponseCallback, reinterpret_cast<void *>(responseUserData));
	}

	virtual void Update()
	{}

	virtual void Terminate()
	{}
protected:
	virtual State GetState() const
	{
		if (this->TestCallbackFlags()) {
			if (this->TestSuccessFlags()) {
				return State::Failed;
			} else {
				return State::Succeeded;
			}
		}
		else {
			return State::Running;
		}
	}
};

void CreateTests(std::vector<Test *> &tests)
{
	tests.push_back(new Test1());
	tests.push_back(new Test2());
	tests.push_back(new Test3());
	tests.push_back(new Test4());
	tests.push_back(new Test5());
}

void DestroyTests(std::vector<Test *> &tests)
{
	for (std::vector<Test *>::iterator it = tests.begin(), end = tests.end(); it != end; ++it) {
		delete *it;
	}
}