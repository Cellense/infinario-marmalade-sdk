#include "Infinario.h"

#include "IwHTTP.h"
#include "IwHashString.h"
#include "IwRandom.h"

#include "s3eMemory.h"
#include "s3eTimer.h"
#include "s3eDevice.h"

#include <iomanip>
#include <string>
#include <sstream>
#include <queue>

const std::string Infinario::Infinario::_requestUri = "http://api.infinario.com/bulk";
const uint32 Infinario::Infinario::_bufferSize = 1024;

Infinario::Infinario::Infinario(const std::string &projectToken, const std::string &customerId)
: _projectToken(projectToken)
, _customerCookie()
, _customerId(customerId)
, _httpClient()
, _isRequestBeingProcessed(false)
, _jsonPosts()
, _buffer(reinterpret_cast<char *>(s3eMalloc(Infinario::_bufferSize + 1)))
, _accumulatedBodyLength(0)
, _accumulatedBodyContent()
{
	IwRandSeed((int32)s3eTimerGetMs());

	std::stringstream sstream;
	sstream << s3eDeviceGetInt(S3E_DEVICE_PPI_LOGICAL) << " "
		<< s3eDeviceGetInt(S3E_DEVICE_PPI) << " "
		<< s3eDeviceGetInt(S3E_DEVICE_SUPPORTS_SUSPEND_RESUME) << " "
		<< s3eDeviceGetInt(S3E_DEVICE_NUM_CPU_CORES) << " "
		<< s3eDeviceGetString(S3E_DEVICE_LOCALE) << " "
		<< s3eDeviceGetInt(S3E_DEVICE_LANGUAGE) << " "
		<< s3eDeviceGetString(S3E_DEVICE_LANGUAGE) << " "
		<< s3eDeviceGetInt(S3E_DEVICE_MEM_FREE) << " "
		<< s3eDeviceGetInt(S3E_DEVICE_MEM_TOTAL) << " "
		<< s3eDeviceGetInt(S3E_DEVICE_BATTERY_LEVEL) << " "
		<< s3eDeviceGetInt(S3E_DEVICE_MAINS_POWER) << " "
		<< s3eDeviceGetString(S3E_DEVICE_CHIPSET) << " "
		<< s3eDeviceGetString(S3E_DEVICE_IMSI) << " "
		<< s3eDeviceGetString(S3E_DEVICE_PHONE_NUMBER) << " "
		<< s3eDeviceGetString(S3E_DEVICE_NAME) << " "
		<< s3eDeviceGetString(S3E_DEVICE_UNIQUE_ID) << " "
		<< s3eDeviceGetString(S3E_DEVICE_ID) << " "
		<< s3eDeviceGetString(S3E_DEVICE_ARCHITECTURE) << " "
		<< s3eDeviceGetString(S3E_DEVICE_OS_VERSION) << " "
		<< s3eDeviceGetString(S3E_DEVICE_OS);

	std::stringstream hashstream;
	hashstream << (IwHashString(sstream.str().c_str()) * IwRandMinMax(1, 1000));
	this->_customerCookie = hashstream.str();

	this->_httpClient.SetProxy("127.0.0.1:8888"); //DEBUG
}

Infinario::Infinario::~Infinario()
{
	s3eFree(reinterpret_cast<void *>(this->_buffer));
}

void Infinario::Infinario::anonymize()
{
	this->_customerId.clear();
}

void Infinario::Infinario::identify(const std::string &customerId, ResponseCallback callback, void *userData)
{
	this->_customerId = customerId;

	std::stringstream bodyStream;
	bodyStream << 
		"{ \"commands\": [{ "
			"\"name\": \"crm/customers\", "
			"\"data\": { "
				"\"ids\": {"
					" \"registered\": \"" << this->_customerId << "\","
					" \"cookie\": \"" << this->_customerCookie << "\" "
				"}, "
				"\"project_id\": \"" << this->_projectToken << "\" "
			"}"
		"}]}";

	IndentifyUserData *identifyUserData = new IndentifyUserData(*this, callback, userData);
	this->enqueueJsonPost(JsonPost(Infinario::_requestUri, bodyStream.str(), 
		Infinario::identifyCallback, reinterpret_cast<void *>(identifyUserData)));
}

void Infinario::Infinario::update(const std::string &customerAttributes, ResponseCallback callback, void *userData)
{
	std::stringstream bodyStream;
	bodyStream << 
		"{ \"commands\": [{ "
			"\"name\": \"crm/customers\", "
			"\"data\": { "
				"\"ids\": { ";
	if (this->_customerId.empty()) {
		bodyStream << "\"cookie\": \"" << this->_customerCookie << "\" ";
	} else {
		bodyStream << "\"registered\": \"" << this->_customerId << "\" ";
	}
	bodyStream <<
				"}, "
				"\"project_id\": \"" << this->_projectToken << "\", "
				"\"properties\": " << customerAttributes <<
			"}"
		"}]}";

	this->enqueueJsonPost(JsonPost(Infinario::_requestUri, bodyStream.str(), callback, userData));
}

void Infinario::Infinario::track(const std::string &eventName, const std::string &eventAttributes,
	ResponseCallback callback, void *userData)
{
	this->track(eventName, eventAttributes, static_cast<double>(s3eTimerGetUTC()) / 1000.0, callback);
}

void Infinario::Infinario::track(const std::string &eventName, const std::string &eventAttributes,
	const double timestamp, ResponseCallback callback, void *userData)
{
	std::stringstream bodyStream;
	bodyStream <<
		"{ \"commands\": [{ "
			"\"name\": \"crm/events\", "
			"\"data\": { "
				"\"customer_ids\": { ";
	if (this->_customerId.empty()) {
		bodyStream << "\"cookie\": \"" << this->_customerCookie << "\" ";
	} else {
		bodyStream << "\"registered\": \"" << this->_customerId << "\" ";
	}
	bodyStream <<
				" }, "
				"\"project_id\": \"" << this->_projectToken << "\", "
				"\"timestamp\": " << std::setprecision(3) << std::fixed << timestamp << ", "
				"\"type\": \"" << eventName << "\", "
				"\"properties\": " << eventAttributes <<
			"}"
		"}]}";

	this->enqueueJsonPost(JsonPost(Infinario::_requestUri, bodyStream.str(), callback, userData));
}

Infinario::Infinario::JsonPost::JsonPost(const std::string &uri, const std::string &body,
	ResponseCallback callback, void *userData)
: _uri(uri)
, _body(body)
, _callback(callback)
, _userData(userData)
{}

Infinario::Infinario::IndentifyUserData::IndentifyUserData(Infinario &infinario,
	ResponseCallback callback, void *userData)
: _infinario(infinario)
, _callback(callback)
, _userData(userData)
{}

void Infinario::Infinario::identifyCallback(const CIwHTTP &httpClient, const ResponseStatus &responseStatus,
	const std::string &responseBody, void *identifyUserData)
{
	IndentifyUserData *identifyData = reinterpret_cast<IndentifyUserData *>(identifyUserData);

	if ((responseStatus != ResponseStatus::Success) ||
		(responseBody.find("\"status\": \"ok\"") == std::string::npos))
	{
		identifyData->_infinario.anonymize();
	}
	if (identifyData->_callback != NULL) {
		identifyData->_callback(httpClient, responseStatus, responseBody, identifyData->_userData);
	}	

	delete identifyData;
}

int32 Infinario::Infinario::recieveHeader(void *systenData, void *userData)
{
	Infinario &infinario = *(reinterpret_cast<Infinario *>(userData));
	JsonPost &lastJsonPost(infinario._jsonPosts.front());

	if (infinario._httpClient.GetStatus() == S3E_RESULT_ERROR) {
		if (lastJsonPost._callback != NULL) {
			lastJsonPost._callback(infinario._httpClient, ResponseStatus::ReceiveHeaderError,
				infinario._accumulatedBodyContent.str(), lastJsonPost._userData);
		}
		infinario._jsonPosts.pop();
		infinario.executeJsonPost();
		return 0;
	}

	// Depending on how the server is communicating the content
	// length, we may actually know the length of the content, or
	// we may know the length of the first part of it, or we may
	// know nothing. ContentExpected always returns the smallest
	// possible size of the content, so allocate that much space
	// for now if it's non-zero. If it is of zero size, the server
	// has given no indication, so we need to guess. We'll guess at 1k.
	infinario._accumulatedBodyLength = infinario._httpClient.ContentExpected();
	if (infinario._accumulatedBodyLength == 0) {
		infinario._accumulatedBodyLength = Infinario::_bufferSize;
	}

	infinario._buffer[infinario._accumulatedBodyLength] = 0;
	infinario._httpClient.ReadDataAsync(infinario._buffer, infinario._accumulatedBodyLength,
		0, Infinario::recieveBody, userData);

	return 0;
}

int32 Infinario::Infinario::recieveBody(void *systenData, void *userData)
{
	Infinario &infinario = *(reinterpret_cast<Infinario *>(userData));
	JsonPost &lastJsonPost(infinario._jsonPosts.front());

	// This is the callback indicating that a ReadDataAsync call has
	// completed.  Either we've finished, or a bigger buffer is
	// needed.  If the correct ammount of data was supplied initially,
	// then this will only be called once. However, it may well be
	// called several times when using chunked encoding.

	// Firstly see if there's an error condition.
	if (infinario._httpClient.GetStatus() == S3E_RESULT_ERROR) {
		if (lastJsonPost._callback != NULL) {
			lastJsonPost._callback(infinario._httpClient, ResponseStatus::RecieveBodyError,
				infinario._accumulatedBodyContent.str(), lastJsonPost._userData);
		}
		infinario._jsonPosts.pop();
		infinario.executeJsonPost();
		return 0;
	}

	infinario._accumulatedBodyContent << std::string(infinario._buffer);

	if (infinario._httpClient.ContentFinished()) {
		if (lastJsonPost._callback != NULL) {
			lastJsonPost._callback(infinario._httpClient, ResponseStatus::Success,
				infinario._accumulatedBodyContent.str(), lastJsonPost._userData);
		}
		infinario._jsonPosts.pop();
		infinario.executeJsonPost();
		return 0;
	}

	// We have some data but not all of it. We need more space.
	uint32 oldReadSize = infinario._accumulatedBodyLength;
	// If iwhttp has a guess how big the next bit of data is (this
	// basically means chunked encoding is being used), allocate
	// that much space. Otherwise guess.
	if (infinario._accumulatedBodyLength < infinario._httpClient.ContentExpected()) {
		infinario._accumulatedBodyLength = infinario._httpClient.ContentExpected();
	} else {
		infinario._accumulatedBodyLength += Infinario::_bufferSize;
	}

	infinario._buffer[infinario._accumulatedBodyLength] = 0;
	infinario._httpClient.ReadDataAsync(infinario._buffer, infinario._accumulatedBodyLength - oldReadSize,
		0, Infinario::recieveBody, userData);

	return 0;
}

void Infinario::Infinario::enqueueJsonPost(const JsonPost &jsonPost)
{
	this->_jsonPosts.push(jsonPost);
	if (!this->_isRequestBeingProcessed) {
		this->_isRequestBeingProcessed = true;
		this->executeJsonPost();
	}
}

void Infinario::Infinario::executeJsonPost()
{	
	if (this->_jsonPosts.empty()) {
		this->_isRequestBeingProcessed = false;
		return;
	}
	
	this->_accumulatedBodyContent.str(std::string());
	this->_accumulatedBodyContent.clear();

	JsonPost &lastJsonPost(this->_jsonPosts.front());

	this->_httpClient.SetRequestHeader("Content-Type", "application/json");
	if (this->_httpClient.Post(lastJsonPost._uri.c_str(), lastJsonPost._body.c_str(),
		static_cast<int32>(lastJsonPost._body.size()), Infinario::recieveHeader,
		reinterpret_cast<void *>(this)) == S3E_RESULT_ERROR)
	{
		if (lastJsonPost._callback != NULL) {
			lastJsonPost._callback(this->_httpClient, ResponseStatus::SendRequestError,
				this->_accumulatedBodyContent.str(), lastJsonPost._userData);
		}
		this->_jsonPosts.pop();
		this->executeJsonPost();
	}
}