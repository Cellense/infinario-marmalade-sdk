#include "Infinario.h"

#include "IwGx.h"
#include "IwGL.h"
#include "IwGxPrint.h"

#include "s3e.h"
#include "s3eFile.h"
#include "s3eKeyboard.h"

#include <string>
#include <sstream>

#define FRAME_RATE 25

#define TEST_PROJECT_TOKEN "c15eb0de-9745-11e5-acc0-b083fedeed2e"
#define TEST_CUSTOMER_ID "example@marmalade.com"
#define TEST_OUTPUT_FILE "output.txt"

const std::string projectToken(TEST_PROJECT_TOKEN);
const std::string customerId(TEST_CUSTOMER_ID);

Infinario::Infinario *infinario1;
Infinario::Infinario *infinario2;
Infinario::Infinario *infinario3;
Infinario::Infinario *infinario4;

bool isPart2Done = true;
int testId = 1;
s3eFile *outputFile;

// Testing callback function which prints the response of the server to a file.
// More information can be obtained from the httpClient object.
void incrementCallback(const CIwHTTP *httpClient, const Infinario::ResponseStatus responseStatus,
	const std::string &responseBody, void *userData)
{
	std::stringstream outputStream;
	outputStream
		<< "Response #" << testId << std::endl
		<< " --- " << std::endl
		<< "Status: ";
	switch (responseStatus)
	{
	case Infinario::ResponseStatus::Success:
		outputStream << "Success";
		break;
	case Infinario::ResponseStatus::SendRequestError:
		outputStream << "SendRequestError";
		break;
	case Infinario::ResponseStatus::ReceiveHeaderError:
		outputStream << "ReceiveHeaderError";
		break;
	case Infinario::ResponseStatus::RecieveBodyError:
		outputStream << "RecieveBodyError";
		break;
	case Infinario::ResponseStatus::KilledError:
		outputStream << "KilledError";
		break;
	default:
		outputStream << "UnknownError";
		break;
	}
	outputStream
		<< std::endl
		<< " --- " << std::endl
		<< "Body:" << std::endl
		<< responseBody << std::endl;
	if (userData != NULL) {
		std::string *stringData = reinterpret_cast<std::string *>(userData);

		outputStream
			<< std::endl
			<< " --- " << std::endl
			<< "UserData:" << std::endl
			<< *stringData << std::endl;

		delete stringData;
	}
	outputStream
		<< " ************************ " << std::endl;

	std::string outputString(outputStream.str());
	s3eFileWrite(reinterpret_cast<const void *>(outputString.c_str()), outputString.size(), 1, outputFile);

	++testId;
}

void SelfDestructor(void *)
{
	delete infinario3;
	s3eFileWrite(reinterpret_cast<const void *>("deleted 3"), 9, 1, outputFile);
}

void ExampleInit()
{
	outputFile = s3eFileOpen(TEST_OUTPUT_FILE, "w");

	if (outputFile != NULL) {
		// Test tracking using constructor with explicit customer identifier.
		infinario1 = new Infinario::Infinario(projectToken, customerId);
		// Testing setting of customer attributes.
		infinario1->Update("{ \"name\": \"Rumbal\", \"age\": 12, \"e-peen\": 1.2364 }", incrementCallback);
		infinario1->Track("omg", "{ \"quest\": \"dragon\", \"loot\" : \"zidane\", \"rly?\" : 52, \"messi\" : 7.41 }",
			1449008100.0, incrementCallback);

		// Test tracking using constructor with anonymous customer that is identified before the event.
		// Also tests customer merging.
		infinario2 = new Infinario::Infinario(projectToken);
		infinario2->Identify(customerId, incrementCallback);
		infinario2->Track("omg", "{ \"quest\": \"ballz\", \"loot\" : \"uwotmate?\", \"rly?\" : 42, \"messi\" : 2.41 }",
			1449008256.0, incrementCallback);

		// Test tracking using constructor with anonymous customer that is identified after the event.
		infinario3 = new Infinario::Infinario(projectToken);
		infinario3->SetEmptyRequestQueueCallback(SelfDestructor);
		infinario3->Track("omg", "{ \"quest\": \"ballzianus\", \"loot\" : \"herpaderba\", \"rly?\" : 4112, \"messi\""
			" : 211.41 }", 1449008523.0, incrementCallback);
		infinario3->Identify(customerId, incrementCallback);

		// Testing update of customer attributes.
		infinario3->Update("{ \"name\": \"NotSoRUmb\", \"noob\" : false, \"e-peen\": 1000.2364 }", incrementCallback);

		// Testing long and short tracking bodies.
		infinario3->Track("bigone", "{ \"name\": \"Get\", \"name2\" : \"Ready\", \"name3\" : \"To\","
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
			" or else he endures pains to avoid worse pains.\" }", 1449008622.0, incrementCallback);
		infinario3->Track("lost", "{ \"huh\": \"0.o\", \"orly\" : \"yrly\" }", 1449008822.0, incrementCallback);

		// Test tracking with no callback.
		infinario3->Track("cram", "{ \"hur\": \"dur\" }", 1449010822.0);

		// Test tracking with callback data.
		std::string *gg = new std::string("gg");
		infinario3->Track("zam", "{ \"mur\": \"xc\" }", 1449010822.0, incrementCallback, reinterpret_cast<void *>(gg));

		// Test tracking without timestamp.
		infinario3->Track("flam", "{ \"hzm\": \"qw\" }", incrementCallback);
		infinario3->Track("ham", "{ \"food\": \"mickeyD's\" }");

		// Testing order of command execution Part1.
		infinario4 = new Infinario::Infinario(projectToken);
		std::string *s1 = new std::string("1");
		infinario4->Track("omg", "{ \"quest\": \"ballzianus\", \"loot\" : \"herpaderba\", \"rly?\" : 4112, \"messi\""
			" : 211.41 }", 1449008523.0, incrementCallback, reinterpret_cast<void *>(s1));
		std::string *s2 = new std::string("2");
		infinario4->Identify(customerId, incrementCallback, reinterpret_cast<void *>(s2));
		std::string *s3 = new std::string("3");
		infinario4->Track("omg", "{ \"quest\": \"ballz\", \"loot\" : \"uwotmate?\", \"rly?\" : 42, \"messi\" : 2.41 }",
			1449008256.0, incrementCallback, reinterpret_cast<void *>(s3));
		std::string *s4 = new std::string("4");
		infinario4->Update("{ \"name\": \"NotSoRUmb\", \"noob\" : false, \"e-peen\": 1000.2364 }", incrementCallback,
			reinterpret_cast<void *>(s4));

		isPart2Done = false;

		// Test killed event.
		{
			Infinario::Infinario infinario(projectToken, "hello@hi.gg");
			infinario.Track("gg", "{ }", incrementCallback, reinterpret_cast<void *>(new std::string("Why so fast?")));
		}

	}

	IwGxInit();
}

void ExampleShutDown()
{
	IwGxTerminate();

	delete infinario1;
	delete infinario2;
	delete infinario4;

	s3eFileClose(outputFile);
}

bool ExampleUpdate()
{
	if ((!isPart2Done) && (testId > 5)) {
		isPart2Done = true;

		// Testing order of command execution Part2.
		std::string *s5 = new std::string("5");
		infinario4->Update("{ \"name\": \"Rumbal\", \"age\": 12, \"e-peen\": 1.2364 }", incrementCallback,
			reinterpret_cast<void *>(s5));
		std::string *s6 = new std::string("6");
		infinario4->Track("omg", "{ \"quest\": \"dragon\", \"loot\" : \"zidane\", \"rly?\" : 52, \"messi\" : 7.41 }",
			1449008100.0, incrementCallback, reinterpret_cast<void *>(s6));
	}

	return true;
}

void ExampleRender()
{
	// Clear screen
	IwGxClear(IW_GX_COLOUR_BUFFER_F | IW_GX_DEPTH_BUFFER_F);

	// Swap buffers
	IwGxFlush();
	IwGxSwapBuffers();
}

//-----------------------------------------------------------------------------
// Main global function
//-----------------------------------------------------------------------------
int main()
{
	const int32 msPerFrame = 1000 / FRAME_RATE;

	// IwGx can be initialised in a number of different configurations to help the linker eliminate unused code.
	// Normally, using IwGxInit() is sufficient.
	// To only include some configurations, see the documentation for IwGxInit_Base(), IwGxInit_GLRender() etc.
	IwGxInit();

	// Example main loop
	ExampleInit();

	// Set screen clear colour
	IwGxSetColClear(0xff, 0xff, 0xff, 0xff);
	IwGxPrintSetColour(128, 128, 128);

	for (;;) {
		s3eDeviceYield(0);
		s3eKeyboardUpdate();

		int64 start = s3eTimerGetMs();

		ExampleUpdate();
		if ((s3eKeyboardGetState(s3eKeyEsc) & S3E_KEY_STATE_DOWN) || s3eDeviceCheckQuitRequest()) {
			break;
		}

		// Clear the screen.
		IwGxClear(IW_GX_COLOUR_BUFFER_F | IW_GX_DEPTH_BUFFER_F);
		ExampleRender();

		// Attempt to lock frame rate.
		while ((s3eTimerGetMs() - start) < msPerFrame) {
			int32 yield = msPerFrame - static_cast<int32>(s3eTimerGetMs() - start);
			if (yield < 0) {
				break;
			}
			s3eDeviceYield(yield);
		}
	}

	ExampleShutDown();
	
	IwGxTerminate();

	return 0;
}
