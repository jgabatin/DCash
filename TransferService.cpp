#define RAPIDJSON_HAS_STDSTRING 1

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

#include <iostream>
#include <map>
#include <string>
#include <sstream>

#include "TransferService.h"
#include "ClientError.h"

#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/istreamwrapper.h"
#include "rapidjson/stringbuffer.h"

using namespace rapidjson;
using namespace std;

TransferService::TransferService() : HttpService("/transfers") { }

void TransferService::post(HTTPRequest *request, HTTPResponse *response) {
  User *check_user = this->getAuthenticatedUser(request);

  if (check_user == NULL) {
    // Invalid token provided
    response->setStatus(400);
    throw ClientError::badRequest();
  }

  WwwFormEncodedDict fields = request->formEncodedBody();
  int amt = stoi(fields.get("amount"));

  if (check_user->balance < amt || amt < 50) {
    // Lack of funds or invalid transfer
    response->setStatus(400);
    throw ClientError::badRequest();
  }

  // Retrieve fields to initiate a transfer
  Transfer *t = new Transfer;
  t->from = check_user;
  t->amount = amt;

  string recipient = fields.get("to");
  User *recip = new User;
  recip = m_db->users[recipient];

  if (recip == NULL) {
    // User is not in our DB
    response->setStatus(404);
    throw ClientError::notFound();
  }
  t->to = recip;

  // Deduct amount from sender and credit the recipient
  check_user->balance -= amt;
  recip->balance += amt;
  // Add the transfer to our database
  m_db->transfers.push_back(t);

  // Create a return object w/ the required fields
  Document document;
  Document::AllocatorType& alloc = document.GetAllocator();
  Value val;
  val.SetObject();
  val.AddMember("balance", check_user->balance, alloc);

  Value array;
  array.SetArray();

  // Get all of the user's transactions
  for (unsigned int i = 0; i < m_db->transfers.size(); i++) {
    Transfer *check_t = m_db->transfers[i];

    if (check_t->from->username == check_user->username) {
      // Get the transfer data
      Value transaction;
      transaction.SetObject();
      transaction.AddMember("from", check_t->from->username, alloc);
      transaction.AddMember("to", check_t->to->username, alloc);
      transaction.AddMember("amount", check_t->amount, alloc);
      array.PushBack(transaction, alloc);
    }
  }

  // Append to the document
  val.AddMember("transfers", array, alloc);
  document.Swap(val);
  StringBuffer buffer;
  PrettyWriter<StringBuffer> writer(buffer);
  document.Accept(writer);

  // Return the response
  response->setContentType("application/json");
  response->setBody(buffer.GetString() + string("\n"));
}
