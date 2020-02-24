// $Id: cix.cpp,v 1.9 2019-04-05 15:04:28-07 - - $

#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <sstream>
using namespace std;

#include <libgen.h>
#include <sys/types.h>
#include <unistd.h>

#include "fstream"
#include "protocol.h"
#include "logstream.h"
#include "sockets.h"

logstream outlog (cout);
struct cix_exit: public exception {};

unordered_map<string,cix_command> command_map {
   {"exit", cix_command::EXIT},
   {"help", cix_command::HELP},
   {"ls"  , cix_command::LS  },
   {"put",  cix_command::PUT},
   {"get", cix_command::GET},
   {"rm", cix_command::RM},
};

static const char help[] = R"||(
exit         - Exit the program.  Equivalent to EOF.
get filename - Copy remote file to local host.
help         - Print help summary.
ls           - List names of files on remote server.
put filename - Copy local file to remote host.
rm filename  - Remove file from remote server.
)||";

void cix_help() {
   cout << help;
}

void cix_ls (client_socket& server) {
   cix_header header;
   header.command = cix_command::LS;
   outlog << "sending header " << header << endl;
   send_packet (server, &header, sizeof header);
   recv_packet (server, &header, sizeof header);
   outlog << "received header " << header << endl;
   if (header.command != cix_command::LSOUT) {
      outlog << "sent LS, server did not return LSOUT" << endl;
      outlog << "server returned " << header << endl;
   }else{
      auto buffer = make_unique<char[]> (header.nbytes +1);
      recv_packet (server, buffer.get(), header.nbytes);
      outlog << "received " << header.nbytes << " bytes" << endl;
      buffer[header.nbytes] = '\0';
      cout << buffer.get();
   }
}

void cix_put(client_socket& server, string filename) {
   cix_header header;
   snprintf(header.filename, filename.length() +1, filename.c_str());
   ifstream content(header.filename, ifstream::binary);

   if(content){
      content.seekg(0, content.end);
      int len = content.tellg();
      content.seekg(0, content.beg);
      unique_ptr<char[]> buffer = make_unique<char[]>(len);
      content.read(buffer.get(), len);
      header.command = cix_command::PUT;
      header.nbytes = len;
      send_packet(server, &header, sizeof header);
      send_packet(server, buffer.get(), len);
      recv_packet(server, &header, sizeof header);
   }else{
      cerr << "Error: file: '" << filename << "'' was not found" << endl;
   }
   if(header.command == cix_command::ACK) {
      cout << "received ACK: put file on server" << endl;
   }else if(header.command == cix_command::NAK) {
      cerr << "received NAK: file was not put on server" << endl;
   }
   content.close();
}

void cix_get(client_socket& server, string filename) {
   cix_header header;
   header.command = cix_command::GET;
   snprintf(header.filename, filename.length() +1, filename.c_str());
   outlog << "sending header " << header << endl;
   send_packet (server, &header, sizeof header);
   recv_packet (server, &header, sizeof header);
   outlog << "received header " << header << endl;
   if(header.command != cix_command::FILEOUT) {
      outlog << filename << " does not exist on the server" << endl;
      outlog << "server returned " << header << endl;
   }else{

      unique_ptr<char[]> buffer = make_unique<char[]>(header.nbytes+1);

      recv_packet (server, buffer.get(), header.nbytes);
      outlog << "received " << header.nbytes << " bytes" << endl;
      buffer[header.nbytes] = '\0';
      ofstream os(header.filename, std::ofstream::binary);
      os.write(buffer.get(), header.nbytes);
   }
}
void cix_rm(client_socket& server, string filename) {
   cix_header header;
   snprintf(header.filename, filename.length()+1, filename.c_str());
   header.command = cix_command::RM;
   header.nbytes = 0;
   send_packet(server, &header, sizeof header);
   recv_packet(server, &header, sizeof header);
   if(header.command == cix_command::ACK) {
      cout << "received ACK: deleted file: " << filename << endl;
   }else if (header.command == cix_command::NAK) {
      cerr << "received NAK: file was not deleted" << endl;
   }
}


void usage() {
   cerr << "Usage: " << outlog.execname() << " [host] [port]" << endl;
   throw cix_exit();
}

int main (int argc, char** argv) {
   vector<string> tokenz;
   outlog.execname (basename (argv[0]));
   outlog << "starting" << endl;
   vector<string> args (&argv[1], &argv[argc]);
   if (args.size() > 2) usage();
   string host = get_cix_server_host (args, 0);
   in_port_t port = get_cix_server_port (args, 1);
   outlog << to_string (hostinfo()) << endl;
   try {
      outlog << "connecting to " << host << " port " << port << endl;
      client_socket server (host, port);
      outlog << "connected to " << to_string (server) << endl;
      for (;;) {
         string line;
         getline (cin, line);
         if (cin.eof()) throw cix_exit();
         std::istringstream ss(line);
         std::string token;
         while(std::getline(ss, token, ' ')) 
         tokenz.push_back(token);
         outlog << "command " << line << endl;
         const auto& itor = command_map.find (tokenz.at(0));
         cix_command cmd = itor == command_map.end()
                         ? cix_command::ERROR : itor->second;
         switch (cmd) {
            case cix_command::EXIT:
               throw cix_exit();
               break;
            case cix_command::HELP:
               cix_help();
               break;
            case cix_command::LS:
               cix_ls (server);
               break;
            case cix_command::PUT:
               cix_put(server, tokenz.at(1));
               break;
            case cix_command::GET:
               cix_get(server, tokenz.at(1));
               break;
            case cix_command::RM:
               cix_rm(server, tokenz.at(1));
               break;
            default:
               outlog << line << ": invalid command" << endl;
               break;
         }
         tokenz.clear();
      }
   }catch (socket_error& error) {
      outlog << error.what() << endl;
   }catch (cix_exit& error) {
      outlog << "caught cix_exit" << endl;
   }
   outlog << "finishing" << endl;
   return 0;
}

