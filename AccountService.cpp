#define RAPIDJSON_HAS_STDSTRING 1

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "AccountService.h"
#include "ClientError.h"

#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/istreamwrapper.h"
#include "rapidjson/stringbuffer.h"

using namespace std;
using namespace rapidjson;

AccountService::AccountService() : HttpService("/users") { }

void AccountService::get(HTTPRequest *request, HTTPResponse *response) {
  User *check_user = this->getAuthenticatedUser(request);
  int bal;
  string mail;

  if (check_user == NULL) {
    // Invalid token provided
    response->setStatus(400);
    throw ClientError::badRequest();
  }
  bal = check_user->balance;
  mail = check_user->email;

  // Create a return object w/ the required fields
  Document document;
  Document::AllocatorType& a = document.GetAllocator();
  Value val;
  val.SetObject();
  val.AddMember("balance", bal, a);
  val.AddMember("email", mail, a);
  document.Swap(val);
  StringBuffer buffer;
  PrettyWriter<StringBuffer> writer(buffer);
  document.Accept(writer);

  // Return the response
  response->setContentType("application/json");
  response->setBody(buffer.GetString() + string("\n"));
}

void AccountService::put(HTTPRequest *request, HTTPResponse *response) {
  User *check_user = this->getAuthenticatedUser(request);
  int bal;
  string mail;

  if (check_user == NULL) {
    // Invalid token provided
    response->setStatus(400);
    throw ClientError::badRequest();
  }

  WwwFormEncodedDict addr = request->formEncodedBody();
  mail = addr.get("email");

  if (mail.length() == 0) {
    // No email provided
    response->setStatus(400);
    throw ClientError::badRequest();
  }
  check_user->email = mail;
  bal = check_user->balance;

  // Create a return object w/ the required fields
  Document document;
  Document::AllocatorType& a = document.GetAllocator();
  Value val;
  val.SetObject();
  val.AddMember("balance", bal, a);
  val.AddMember("email", mail, a);
  document.Swap(val);
  StringBuffer buffer;
  PrettyWriter<StringBuffer> writer(buffer);
  document.Accept(writer);

  // Return the response
  response->setContentType("application/json");
  response->setBody(buffer.GetString() + string("\n"));
}
