#define RAPIDJSON_HAS_STDSTRING 1

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "AuthService.h"
#include "StringUtils.h"
#include "ClientError.h"

#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/istreamwrapper.h"
#include "rapidjson/stringbuffer.h"

using namespace std;
using namespace rapidjson;

AuthService::AuthService() : HttpService("/auth-tokens") { }

void AuthService::post(HTTPRequest *request, HTTPResponse *response) {
  WwwFormEncodedDict user_fields = request->formEncodedBody();
  string u_name = user_fields.get("username");
  string u_pass = user_fields.get("password");
  string a_token, id;

  if (u_name.length() == 0 || u_pass.length() == 0) {
    // Did not provide a username or password
    response->setStatus(400);
    throw ClientError::badRequest();
  }

  for (unsigned int i = 0; i < u_name.length(); i++) {
    if (isupper(u_name[i])) {
      // Uppercase char found in username
      response->setStatus(400);
      throw ClientError::badRequest();
    }
  }

  User *check_user = m_db->users[u_name];

  if (check_user != NULL) {
    // Registered user; check if passwords match
    if (check_user->password != u_pass) {
      response->setStatus(403);
      throw ClientError::forbidden();
    }

    // Generate a new auth token for this session
    a_token = StringUtils::createAuthToken();
    m_db->auth_tokens[a_token] = check_user;
    id = check_user->user_id;
    response->setStatus(200);
  } else {
      // New user; add info to the DB
      check_user = new User;
      check_user->username = u_name;
      check_user->password = u_pass;
      check_user->user_id = StringUtils::createUserId();
      check_user->balance = 0;

      id = check_user->user_id;
      a_token = StringUtils::createAuthToken();

      m_db->users[u_name] = check_user;
      m_db->auth_tokens[a_token] = check_user;

      response->setStatus(201);
  }

  // Create a return object w/ the required fields
  Document document;
  Document::AllocatorType& a = document.GetAllocator();
  Value val;
  val.SetObject();
  val.AddMember("auth_token", a_token, a);
  val.AddMember("user_id", id, a);
  document.Swap(val);
  StringBuffer buffer;
  PrettyWriter<StringBuffer> writer(buffer);
  document.Accept(writer);

  // Return the response
  response->setContentType("application/json");
  response->setBody(buffer.GetString() + string("\n"));
}

void AuthService::del(HTTPRequest *request, HTTPResponse *response) {
  User *check_user = this->getAuthenticatedUser(request);

  if (check_user == NULL) {
    // Invalid token provided
    response->setStatus(400);
    throw ClientError::badRequest();
  }

  string a_token = request->getUrl();
  a_token = a_token.substr(a_token.find_last_of('/') + 1, a_token.length());

  if (check_user != m_db->auth_tokens[a_token]) {
    // Attempted to delete another user's auth token
    response->setStatus(401);
    throw ClientError::unauthorized();
  }

  // Log the user out of this session
  m_db->auth_tokens[a_token] = NULL;
}
