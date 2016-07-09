#include "uploader.h"

using boost::asio::ip::tcp;
namespace ssl = boost::asio::ssl;
typedef ssl::stream<tcp::socket> ssl_socket;



///@brief Helper class that prints the current certificate's subject
///       name and the verification results.
template <typename Verifier>
class verbose_verification
{
public:
  verbose_verification(Verifier verifier)
    : verifier_(verifier)
  {}

  bool operator()(
    bool preverified,
    boost::asio::ssl::verify_context& ctx
  )
  {
    char subject_name[256];
    X509* cert = X509_STORE_CTX_get_current_cert(ctx.native_handle());
    X509_NAME_oneline(X509_get_subject_name(cert), subject_name, 256);
    bool verified = verifier_(preverified, ctx);
    std::cout << "Verifying: " << subject_name << "\n"
                 "Verified: " << verified << std::endl;
    return verified;
  }
private:
  Verifier verifier_;
};

///@brief Auxiliary function to make verbose_verification objects.
template <typename Verifier>
verbose_verification<Verifier>
make_verbose_verification(Verifier verifier)
{
  return verbose_verification<Verifier>(verifier);
}


      void add_post_field(std::ostringstream &post_stream, std::string name, std::string value, std::string boundary) {
        post_stream << "\r\n--" << boundary << "\r\n";
        post_stream << "Content-Disposition: form-data; name=\"" << name << "\"\r\n";
        post_stream << "\r\n";
        post_stream << value;
      }




      inline std::string to_string (long l)
      {
          std::stringstream ss;
          ss << l;
          return ss.str();
      }



void build_request(struct call_data *call,   boost::asio::streambuf &request_) {
    //boost::asio::streambuf request_;
    std::string server = "api.openmhz.com";
    std::string path =  "/upload";
      std::string boundary("MD5_0be63cda3bf42193e4303db2c5ac3138");


      std::string form_name("call");
      std::string form_filename(call->converted);

      std::ifstream file(call->filename, std::ios::binary);

         // Make sure we have something to read.
     if ( !file.is_open() ) {
       std::cout << "Error opening file \n";
         //throw (std::exception("Could not open file."));
     }

         // Copy contents "as efficiently as possible".




         //------------------------------------------------------------------------
         // Create Disposition in a stringstream, because we need Content-Length...
         std::ostringstream oss;
         oss << "--" << boundary << "\r\n";
         oss << "Content-Disposition: form-data; name=\"" << form_name << "\"; filename=\"" << form_filename << "\"\r\n";
         //oss << "Content-Type: text/plain\r\n";
         oss << "Content-Type: application/octet-stream\r\n";
         oss << "Content-Transfer-Encoding: binary\r\n";
         oss << "\r\n";

         oss << file.rdbuf();
         file.close();

         std::string source_list;
         for (int i=0; i < call->source_count; i++ ){
           source_list = source_list + to_string(call->source_list[i]);
           if (i < (call->source_count - 1)) {
             source_list = source_list + ", ";
           }
         }

         add_post_field(oss, "freq", to_string(call->freq), boundary);
         add_post_field(oss, "start_time", to_string(call->start_time), boundary);
         add_post_field(oss, "talkgroup", to_string(call->talkgroup), boundary);
         add_post_field(oss, "emergency", to_string(call->emergency), boundary);
         add_post_field(oss, "source_list", source_list, boundary);

         oss << "\r\n--" << boundary << "--\r\n";

         //------------------------------------------------------------------------

         boost::asio::streambuf post;
       std::ostream post_stream(&request_);
       post_stream << "POST " << path << "" << " HTTP/1.1\r\n";
         post_stream << "Content-Type: multipart/form-data; boundary=" << boundary << "\r\n";
       post_stream << "User-Agent: OpenWebGlobe/1.0\r\n";
         post_stream << "Host: " << server << "\r\n";   // The domain name of the server (for virtual hosting), mandatory since HTTP/1.1
         post_stream << "Accept: */*\r\n";
         post_stream << "Connection: Close\r\n";
         post_stream << "Cache-Control: no-cache\r\n";
         post_stream << "Content-Length: " << oss.str().size() << "\r\n";
         post_stream << "\r\n";

         post_stream << oss.str();

  }

int http_upload(struct call_data *call)
{
  try
{
      boost::asio::io_service io_service;

      // Get a list of endpoints corresponding to the server name.
      tcp::resolver resolver(io_service);
      tcp::resolver::query query("localhost","3005",tcp::resolver::query::canonical_name);
      tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);

      // Try each endpoint until we successfully establish a connection.
      tcp::socket socket(io_service);
      boost::asio::connect(socket, endpoint_iterator);

      // Form the request. We specify the "Connection: close" header so that the
      // server will close the socket after transmitting the response. This will
      // allow us to treat all data up until the EOF as the content.

      boost::asio::streambuf request_;
      build_request(call, request_);
      // Send the request.
      boost::asio::write(socket, request_);

      // Read the response status line. The response streambuf will automatically
      // grow to accommodate the entire line. The growth may be limited by passing
      // a maximum size to the streambuf constructor.
      boost::asio::streambuf response;
      boost::asio::read_until(socket, response, "\r\n");

      // Check that response is OK.
      std::istream response_stream(&response);
      std::string http_version;
      response_stream >> http_version;
      unsigned int status_code;
      response_stream >> status_code;
      std::string status_message;
      std::getline(response_stream, status_message);
      if (!response_stream || http_version.substr(0, 5) != "HTTP/")
      {
        std::cout << "Invalid response\n";
        return 1;
      }
      if (status_code != 200)
      {
        std::cout << "Response returned with status code " << status_code << "\n";
        return 1;
      }

      // Read the response headers, which are terminated by a blank line.
      boost::asio::read_until(socket, response, "\r\n\r\n");

      // Process the response headers.
      std::string header;
      while (std::getline(response_stream, header) && header != "\r")
        std::cout << header << "\n";
      std::cout << "\n";

      // Write whatever content we already have to output.
      if (response.size() > 0)
        std::cout << &response;

      // Read until EOF, writing data to output as we go.
      boost::system::error_code error;
      while (boost::asio::read(socket, response,
            boost::asio::transfer_at_least(1), error))
        std::cout << &response;
      if (error != boost::asio::error::eof)
        throw boost::system::system_error(error);
    }
    catch (std::exception& e)
    {
      std::cout << "Exception: " << e.what() << "\n";
    }
return 0;
}

int https_upload(struct call_data *call)
{
  std::string server = "api.openmhz.com";
  std::string path =  "/upload";
  try
  {


    boost::asio::io_service io_service;
  boost::asio::streambuf response_;

boost::asio::streambuf request_;
build_request(call, request_);



   // Open the file for the shortest time possible.


    // Create a context that uses the default paths for
    // finding CA certificates.
    ssl::context ctx(ssl::context::sslv23);

    ctx.set_default_verify_paths();
    boost::system::error_code ec;
    // Get a list of endpoints corresponding to the server name.
    tcp::resolver resolver(io_service);


    //tcp::resolver::query query("localhost","3005",tcp::resolver::query::canonical_name);
    tcp::resolver::query query(server, "https");
    tcp::resolver::iterator endpoint_iterator = resolver.resolve(query, ec);
    ssl_socket socket(io_service,ctx);

    if (ec)
    {
      std::cout << "Error resolve: " << ec.message() << "\n";
      exit(1);
  }
        std::cout << "Resolve OK" << "\n";
        socket.set_verify_mode(boost::asio::ssl::verify_peer);
        //socket.set_verify_callback(boost::bind(&client::verify_certificate, this, _1, _2));
        //socket.set_verify_callback(ssl::rfc2818_verification(server));
        socket.set_verify_callback(make_verbose_verification(boost::asio::ssl::rfc2818_verification(server)));
        boost::asio::connect(socket.lowest_layer(), endpoint_iterator, ec);
  if (ec)
        {
            std::cout << "Connect failed: " << ec.message() << "\n";
            exit(1);
        }

        std::cout << "Connect OK " << "\n";
        socket.handshake(boost::asio::ssl::stream_base::client, ec);
if (ec)
{
    std::cout << "Handshake failed: " << ec.message() << "\n";
    //exit(1);
} else {
std::cout << "Handshake OK " << "\n";
}
std::cout << "Request: " << "\n";
const char* req_header=boost::asio::buffer_cast<const char*>(request_.data());
std::cout << req_header << "\n";



// The handshake was successful. Send the request.
boost::asio::write(socket, request_, ec);
if (ec)
{
    std::cout << "Error write req: " << ec.message() << "\n";
}

std::cout << &request_;

    // Read the response status line. The response streambuf will automatically
    // grow to accommodate the entire line. The growth may be limited by passing
    // a maximum size to the streambuf constructor.
    boost::asio::streambuf response;
    boost::asio::read_until(socket, response, "\r\n");

    // Check that response is OK.
    std::istream response_stream(&response);
    std::string http_version;
    response_stream >> http_version;
    unsigned int status_code;
    response_stream >> status_code;
    std::string status_message;
    std::getline(response_stream, status_message);
    if (!response_stream || http_version.substr(0, 5) != "HTTP/")
    {
      std::cout << "Invalid response\n";
      return 1;
    }
    if (status_code != 200)
    {
      std::cout << "Response returned with status code " << status_code << "\n";
      return 1;
    }

    // Read the response headers, which are terminated by a blank line.
    boost::asio::read_until(socket, response, "\r\n\r\n");

    // Process the response headers.
    std::string header;
    while (std::getline(response_stream, header) && header != "\r")
      std::cout << header << "\n";
    std::cout << "\n";

    // Write whatever content we already have to output.
    if (response.size() > 0)
      std::cout << &response;

    // Read until EOF, writing data to output as we go.
    boost::system::error_code error;
    while (boost::asio::read(socket, response,
          boost::asio::transfer_at_least(1), error))
      std::cout << &response;
    if (error != boost::asio::error::eof)
      throw boost::system::system_error(error);
  }
  catch (std::exception& e)
  {
    std::cout << "Exception: " << e.what() << "\n";
  }

  return 0;

}

void *convert_upload_call(void *thread_arg){
  struct call_data *call_info;
  char shell_command[400];

  call_info = (struct call_data *) thread_arg;
  boost::filesystem::path m4a(call_info->filename);
  m4a = m4a.replace_extension(".m4a");
  strcpy(call_info->converted, m4a.string().c_str());
  sprintf(shell_command,"ffmpeg -i %s  -c:a libfdk_aac -b:a 32k -cutoff 18000 %s", call_info->filename, m4a.string().c_str());
  std::cout << "Converting: " << call_info->converted << "\n";
  std::cout << "Command: " << shell_command << "\n";
  int rc = system(shell_command);
  https_upload(call_info);
  pthread_exit(NULL);
}

void send_call(Call *call) {
  struct call_data *call_info = (struct call_data *) malloc(sizeof(struct call_data));
  pthread_t thread;
  long *source_list = call->get_source_list();
  call_info->talkgroup = call->get_talkgroup();
	call_info->freq = call->get_freq();
  call_info->encrypted = call->get_encrypted();
  call_info->emergency = call->get_emergency();
  call_info->tdma = call->get_tdma();
  call_info->source_count = call->get_source_count();
  call_info->start_time = call->get_start_time();
  strcpy(call_info->filename, call->get_filename());
  for (int i=0; i < call_info->source_count; i++ ){
    call_info->source_list[i] = source_list[i];
  }
  std::cout << "Creating Upload Thread\n";
  int rc = pthread_create(&thread, NULL, convert_upload_call,(void *) call_info);
  if (rc){
   printf("ERROR; return code from pthread_create() is %d\n", rc);
   exit(-1);
  }
}