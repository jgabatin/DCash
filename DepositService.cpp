#define RAPIDJSON_HAS_STDSTRING 1

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

#include <iostream>
#include <map>
#include <string>
#include <sstream>

#include "DepositService.h"
#include "Database.h"
#include "ClientError.h"
#include "HTTPClientResponse.h"
#include "HttpClient.h"

#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/istreamwrapper.h"
#include "rapidjson/stringbuffer.h"

using namespace rapidjson;
using namespace std;

DepositService::DepositService() : HttpService("/deposits") { }

void DepositService::post(HTTPRequest *request, HTTPResponse *response) {
  User *check_user = this->getAuthenticatedUser(request);

  if (check_user == NULL) {
    // Invalid token provided
    response->setStatus(400);
    throw ClientError::badRequest();
  }

  Deposit *d = new Deposit;
  d->to = check_user;

  // Retrieve fields to initiate a deposit
  WwwFormEncodedDict fields = request->formEncodedBody();
  int amt = stoi(fields.get("amount"));

  if (amt < 50) {
    // Lack of funds or invalid transfer (Stripe min. deposit > $0.50)
    response->setStatus(400);
    throw ClientError::badRequest();
  }
  d->amount = amt;

  string default_curr("usd");
  string stripe_tok = fields.get("stripe_token");

  if (stripe_tok.length() == 0 || stripe_tok.find("tok_") == std::string::npos) {
    // Invalid Stripe token
    response->setStatus(400);
    throw ClientError::badRequest();
  }

  // Establish connection to Stripe
  HttpClient client("api.stripe.com", 443, true);
  client.set_basic_auth(m_db->stripe_secret_key, "");

  // Set and encode the body to initiate the charge
  WwwFormEncodedDict body;
  body.set("amount", amt);
  body.set("currency", default_curr);
  body.set("source", stripe_tok);
  string encoded_body = body.encode();

  // Send the request and get the charge ID
  HTTPClientResponse *client_response = client.post("/v1/charges", encoded_body);

  Document *doc = client_response->jsonBody();
  StringBuffer str_buf;
  PrettyWriter<StringBuffer> write(str_buf);
  doc->Accept(write);
  string check_id = str_buf.GetString();

  if (check_id.find("id") == std::string::npos) {
    // Transaction error
    response->setStatus(400);
    throw ClientError::badRequest();
  }
  string charge_id = (*doc)["id"].GetString();

  // Credit the user's balance
  check_user->balance += amt;
  // Add the deposit to our DB
  d->stripe_charge_id = charge_id;
  m_db->deposits.push_back(d);

  // Create a return object w/ the required fields
  Document document;
  Document::AllocatorType& alloc = document.GetAllocator();
  Value val;
  val.SetObject();
  val.AddMember("balance", check_user->balance, alloc);

  Value array;
  array.SetArray();

  // Get all of the user's transactions
  for (unsigned int i = 0; i < m_db->deposits.size(); i++) {
    Deposit *check_dep = m_db->deposits[i];

    if (check_dep->to->username == check_user->username) {
      // Get the deposit data
      Value transaction;
      transaction.SetObject();
      transaction.AddMember("to", check_dep->to->username, alloc);
      transaction.AddMember("amount", check_dep->amount, alloc);
      transaction.AddMember("stripe_charge_id", check_dep->stripe_charge_id, alloc);
      array.PushBack(transaction, alloc);
    }
  }

  // Append to the document
  val.AddMember("deposits", array, alloc);
  document.Swap(val);
  StringBuffer buffer;
  PrettyWriter<StringBuffer> writer(buffer);
  document.Accept(writer);

  // Return the response
  response->setContentType("application/json");
  response->setBody(buffer.GetString() + string("\n"));
}
