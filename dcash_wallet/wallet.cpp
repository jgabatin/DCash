// wallet.cpp
// Interacts with the local Gunrock server and the Stripe API to simulate a client wallet for multiple users
// Written by John Gabatin

#define RAPIDJSON_HAS_STDSTRING 1

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cctype>

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#include "WwwFormEncodedDict.h"
#include "HttpClient.h"

#include "rapidjson/document.h"

using namespace rapidjson;

int API_SERVER_PORT = 8080;
std::string API_SERVER_HOST = "localhost";
std::string PUBLISHABLE_KEY = "";

std::string auth_token;
std::string user_id;

void OutputError(std::string cause) {
  const std::string error_msg = "Error: ";
  std::cerr << error_msg << cause << std::endl;
}

void OutputBalance(int bal) {
 int dollars = bal / 100;
 int cents = bal % 100;

 std::cout << "Balance: ";
 if (cents == 0) {
   // Ex: 70.00
  std::cout << "$" << dollars << "." << "00" << std::endl;
 } else if (cents < 10) {
    // Ex: 70.05
     std::string dec = "0" + std::to_string(cents);
     std::cout << "$" << dollars << "." << dec << std::endl;
 } else {
    // Ex: 70.75
     std::cout << "$" << dollars << "." << cents << std::endl;
 }
}

std::string GetStripeToken(std::string card, std::string expyr, std::string expmo, std::string cvc) {
  // Set fields needed for the deposit request to Stripe
  WwwFormEncodedDict depo_args;
  depo_args.set("card[number]", card);
  depo_args.set("card[exp_month]", expmo);
  depo_args.set("card[exp_year]", expyr);
  depo_args.set("card[cvc]", cvc);
  std::string depo_info = depo_args.encode();

  // Call Stripe and get the charge ID
  HttpClient stripe_client("api.stripe.com", 443, true);
  stripe_client.set_header("Authorization", std::string("Bearer ") + PUBLISHABLE_KEY);
  HTTPClientResponse *depo_resp = stripe_client.post("/v1/tokens", depo_info);

  if (!depo_resp->success()) {
    return "";
  }

  Document *d_token = depo_resp->jsonBody();
  std::string charge_token = (*d_token)["id"].GetString();

  return charge_token;
}

int GetJSONBalance(HTTPClientResponse *client_resp) {
  Document *body = client_resp->jsonBody();
  int balance = (*body)["balance"].GetInt();

  return balance;
}

int GetBalance(void) {
  const std::string b_path = "/users/" + user_id;

  // Retrieve balance from our Gunrock DB
  HttpClient b_client("localhost", API_SERVER_PORT, false);
  b_client.set_header("x-auth-token", auth_token);
  HTTPClientResponse *b_resp = b_client.get(b_path);

  int balance = GetJSONBalance(b_resp);

  return balance;
}

void UserBalance(void) {
  int balance = GetBalance();
  OutputBalance(balance);
}

void UserTransfer(std::vector<std::string> cmds) {
  const std::string fund_err = "Transfer amount exceeds current user balance.";
  const std::string t_err = "Transfer request is unsuccessful.";

  const std::string recipient = cmds[1];
  std::string amount = cmds[2];
  int user_bal = GetBalance();

  // Convert amount to an int (syntax)
  int deci_pos = amount.find('.');
  amount = amount.erase(deci_pos, 1);

  if (user_bal < std::stoi(amount)) {
    // Insufficient funds
    OutputError(fund_err);
    return;
  }

  // Set fields needed for the transfer request to Gunrock
  WwwFormEncodedDict t_body;
  t_body.set("amount", amount);
  t_body.set("to", recipient);
  std::string t_req_info = t_body.encode();

  // Service the transfer POST request
  HttpClient t_client("localhost", API_SERVER_PORT, false);
  t_client.set_header("x-auth-token", auth_token);
  HTTPClientResponse *t_resp = t_client.post("/transfers", t_req_info);

  if (!t_resp->success()) {
    OutputError(t_err);
    return;
  }

  int balance = GetJSONBalance(t_resp);
  OutputBalance(balance);
}

void UserDeposit(std::vector<std::string> cmds) {
  const std::string tok_err = "Could not get a valid transaction token.";
  const std::string dep_err = "Deposit request is unsuccessful.";

  std::string amount = cmds[1];
  const std::string card_num = cmds[2];
  const std::string exp_year = cmds[3];
  const std::string exp_month = cmds[4];
  const std::string cvc = cmds[5];
  std::string token = GetStripeToken(card_num, exp_year, exp_month, cvc);

  if (token.length() == 0) {
    OutputError(tok_err);
    return;
  }

  // Convert amount to an int (syntax)
  int deci_pos = amount.find('.');
  amount = amount.erase(deci_pos, 1);

  // Set fields needed for the deposit request to Stripe
  WwwFormEncodedDict d_body;
  d_body.set("amount", amount);
  d_body.set("stripe_token", token);
  std::string d_req_info = d_body.encode();

  // Service the deposit POST request
  HttpClient d_client("localhost", API_SERVER_PORT, false);
  d_client.set_header("x-auth-token", auth_token);
  HTTPClientResponse *d_resp = d_client.post("/deposits", d_req_info);

  if (!d_resp->success()) {
    OutputError(dep_err);
    return;
  }

  int balance = GetJSONBalance(d_resp);
  OutputBalance(balance);
}

void UserAuth(std::vector<std::string> cmds) {
  const std::string err = "Could not authenticate user.";
  const std::string username = cmds[1];
  const std::string password = cmds[2];
  const std::string email = cmds[3];

  // Set fields needed to authenticate the user
  WwwFormEncodedDict auth_body;
  auth_body.set("password", password);
  auth_body.set("username", username);
  std::string login_info = auth_body.encode();

  // Service the authentication POST request
  HttpClient auth_client("localhost", API_SERVER_PORT, false);
  HTTPClientResponse *auth_resp = auth_client.post("/auth-tokens", login_info);

  if (!auth_resp->success()) {
    OutputError(err);
    return;
  }

  // Parse the response
  Document *doc = auth_resp->jsonBody();
  auth_token = (*doc)["auth_token"].GetString();
  user_id = (*doc)["user_id"].GetString();

  // Set the fields needed to update the email
  const std::string e_path = "/users/" + user_id;
  WwwFormEncodedDict e_body;
  e_body.set("email", email);
  std::string email_info = e_body.encode();

  // Service the PUT request
  HttpClient e_client("localhost", API_SERVER_PORT, false);
  e_client.set_header("x-auth-token", auth_token);
  HTTPClientResponse *e_resp = e_client.put(e_path, email_info);

  int balance = GetJSONBalance(e_resp);
  OutputBalance(balance);
}

void CallCommand(std::string valid_cmd) {
  const std::string err = "Could not execute command.";
  std::vector<std::string> parsed_cmd;
  std::istringstream s(valid_cmd);
  std::string buf;

  while (s >> buf) {
    parsed_cmd.push_back(buf);
  }

  const std::string prim_cmd = parsed_cmd[0];

  if (prim_cmd == "auth") {
    UserAuth(parsed_cmd);
  } else if (prim_cmd == "balance") {
      UserBalance();
  } else if (prim_cmd == "deposit") {
      UserDeposit(parsed_cmd);
  } else if (prim_cmd == "send") {
      UserTransfer(parsed_cmd);
  } else {
      OutputError(err);
  }
}

bool ValidCommand(std::string command) {
  // Empty / ws input
  if (command.empty() || command.find_first_not_of(' ') == std::string::npos) {
    return false;
  }

  int arg_count = -1;
  std::vector<std::string> parsed_cmd;
  std::istringstream s(command);
  std::string buf;

  while (s >> buf) {
    parsed_cmd.push_back(buf);
    arg_count++;
  }

  const std::string prim_cmd = parsed_cmd[0];

  if (prim_cmd == "auth") {
    // Auth: check that uname, pass, and email were provided
    if (arg_count != 3) {
      return false;
    }

    std::string u_name = parsed_cmd[1];

    // Auth: check that uname is all lowercase
    for (unsigned int i = 0; i < u_name.length(); i++) {
      if (isupper(u_name[i])) {
        return false;
      }
    }
  } else if (prim_cmd == "balance") {
      // Bal: check that no args were provided
      if (arg_count != 0) {
        return false;
      }
  } else if (prim_cmd == "deposit") {
      // Dep: check that amt, cc, exp yr, exp mo, and cvc were provided
      if (arg_count != 5) {
        return false;
      }

      // Dep: check that amt is a positive number
      if (std::stod(parsed_cmd[1]) < 0.50) {
        return false;
      }

      std::string check_cc = parsed_cmd[2];

      // Dep: check that cc length is valid
      if (check_cc.length() != 16) {
        return false;
      }
  } else if (prim_cmd == "send") {
      // Send: check that recipient and amt were provided
      if (arg_count != 2) {
        return false;
      }

      // Send: check that amt is a positive number
      if (std::stoi(parsed_cmd[2]) < 0) {
        return false;
      }
  } else {
      // Invalid command
      return false;
  }

  return true;
}

void Batch(const char* argv) {
  const std::string def_err = "Invalid command provided.";
  const std::string err_file = "Could not open batch file.";
  std::ifstream file(argv);

  if (!file) {
    OutputError(err_file);
    return;
  }

  std::string input;

  while (std::getline(file, input)) {
    const std::string exit_str = "logout";

    // Handle logout
    if (input.find(exit_str) != std::string::npos) {
      exit(0);
    }

    if (ValidCommand(input)) {
      CallCommand(input);
    } else {
        OutputError(def_err);
    }
  }
}

void Interactive(void) {
  const std::string err = "Invalid command provided.";
  const std::string prompt = "D$> ";
  std::string user_input;

  std::cout << prompt;
  while (std::getline(std::cin, user_input, '\n')) {
    const std::string exit_str = "logout";

    // Handle logout
    if (user_input.find(exit_str) != std::string::npos) {
      exit(0);
    }

    if (ValidCommand(user_input)) {
      CallCommand(user_input);
    } else {
        OutputError(err);
    }
    std::cout << prompt;
  }
}

int main(int argc, char *argv[]) {
  int fd = open("config.json", O_RDONLY);
  if (fd < 0) {
    OutputError("Could not open config.json.");
    exit(1);
  }

  // Read and parse the key
  int ret;
  char buffer[4096];
  std::stringstream config;
  while ((ret = read(fd, buffer, sizeof(buffer))) > 0) {
    config << std::string(buffer, ret);
  }

  Document doc;
  doc.Parse(config.str());
  PUBLISHABLE_KEY = doc["stripe_publishable_key"].GetString();

  if (argc > 2) {
    std::cerr << "Usage: {Interactive} ./dcash | {Batch} ./dcash [batch_file]" << std::endl;
  } else if (argc == 2) {
      Batch(argv[1]);
  } else {
      Interactive();
  }

  return 0;
}
