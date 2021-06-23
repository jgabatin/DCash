#include <iostream>

#include <stdlib.h>
#include <stdio.h>

#include "HttpService.h"
#include "ClientError.h"

using namespace std;

HttpService::HttpService(string pathPrefix) {
  this->m_pathPrefix = pathPrefix;
}

User *HttpService::getAuthenticatedUser(HTTPRequest *request)  {
  string token;

  if (request->hasAuthToken()) {
    // Get the inputted token
    token = request->getAuthToken();
  } else {
    // Cannot lookup the user
    throw ClientError::notFound();
  }

  User *check_user = m_db->auth_tokens[token];

  if (check_user == NULL) {
    // User does not exist or was logged out
    throw ClientError::notFound();
  }

  string id = request->getUrl();

  if (id.find("users") != std::string::npos) {
    // Parse and get the ID
    id = id.substr(id.find_last_of('/') + 1, id.length());
    if (check_user->user_id != id) {
      // Mismatched IDs
      throw ClientError::unauthorized();
    }
  }

  return check_user;
}

string HttpService::pathPrefix() {
  return m_pathPrefix;
}

void HttpService::head(HTTPRequest *request, HTTPResponse *response) {
  cout << "HEAD " << request->getPath() << endl;
  throw ClientError::methodNotAllowed();
}

void HttpService::get(HTTPRequest *request, HTTPResponse *response) {
  cout << "GET " << request->getPath() << endl;
  throw ClientError::methodNotAllowed();
}

void HttpService::put(HTTPRequest *request, HTTPResponse *response) {
  cout << "PUT " << request->getPath() << endl;
  throw ClientError::methodNotAllowed();
}

void HttpService::post(HTTPRequest *request, HTTPResponse *response) {
  cout << "POST " << request->getPath() << endl;
  throw ClientError::methodNotAllowed();
}

void HttpService::del(HTTPRequest *request, HTTPResponse *response) {
  cout << "DELETE " << request->getPath() << endl;
  throw ClientError::methodNotAllowed();
}
