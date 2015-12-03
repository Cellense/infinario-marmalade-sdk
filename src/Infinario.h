#ifndef INFINARIO_INFIANRIO_H
#define INFINARIO_INFIANRIO_H

#include "IwHTTP.h"

#include <string>
#include <sstream>
#include <queue>

namespace Infinario
{
	enum class ResponseStatus : char
	{
		Success = 0,
		SendRequestError = 1,
		ReceiveHeaderError = 2,
		RecieveBodyError = 3
	};

	typedef void (* ResponseCallback)(const CIwHTTP &httpClient, const ResponseStatus &responseStatus,
		const std::string &responseBody, void *userData);
	
	class Infinario
	{
	public:
		Infinario(const std::string &projectToken, const std::string &customerId = std::string());
		~Infinario();

		void anonymize();

		void identify(const std::string &customerId, ResponseCallback callback = NULL, void *userData = NULL);

		void update(const std::string &customerAttributes, ResponseCallback callback = NULL, void *userData = NULL);

		void track(const std::string &eventName, const std::string &eventAttributes,
			ResponseCallback callback = NULL, void *userData = NULL);
		void track(const std::string &eventName, const std::string &eventAttributes, const double timestamp,
			ResponseCallback callback = NULL, void *userData = NULL);
	private:		
		class JsonPost
		{
		public:
			JsonPost(const std::string &uri, const std::string &body, ResponseCallback callback, void *userData);

			const std::string _uri;
			const std::string _body;
			ResponseCallback _callback;
			void *_userData;
		};

		class IndentifyUserData
		{
		public:
			IndentifyUserData(Infinario &infinario, ResponseCallback callback, void *userData);

			Infinario &_infinario;
			ResponseCallback _callback;
			void *_userData;
		};
	
		static void identifyCallback(const CIwHTTP &httpClient, const ResponseStatus &responseStatus,
			const std::string &responseBody, void *identifyUserData);
		static int32 recieveHeader(void* systenData, void* userData);
		static int32 recieveBody(void* systenData, void* userData);
		
		static const std::string _requestUri;
		static const uint32 _bufferSize;

		void enqueueJsonPost(const JsonPost &jsonPost);
		void executeJsonPost();
		
		const std::string _projectToken;

		std::string _customerCookie;
		std::string _customerId;

		CIwHTTP _httpClient;

		bool _isRequestBeingProcessed;
		std::queue<JsonPost> _jsonPosts;

		char *_buffer;
		uint32 _accumulatedBodyLength;
		std::stringstream _accumulatedBodyContent;
	};
}

#endif // INFINARIO_INFIANRIO_H