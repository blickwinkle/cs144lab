#include <stdexcept>
#include <string>

#include "byte_stream.hh"

using namespace std;

ByteStream::ByteStream( uint64_t capacity ) : capacity_( capacity ), writed_(0), readed_(0), closed_(false), error_(false), buffer_() {}

void Writer::push( string data )
{
  // Your code here.
  // Push data to stream, but only as much as available capacity allows.
  if (closed_) {
    set_error();
    return;
  }
  if (data.length() > available_capacity()) {
    data = data.substr(0, available_capacity());
  }
  writed_ += data.length();
  for (auto c : data) {
    buffer_.push(c);
  }
}

void Writer::close()
{
  closed_ = true;
}

void Writer::set_error()
{
  error_ = true;
}

bool Writer::is_closed() const
{
  // Your code here.
  return closed_;
}

uint64_t Writer::available_capacity() const
{
  // Your code here.
  return capacity_ - buffer_.size();
}

uint64_t Writer::bytes_pushed() const
{
  // Your code here.
  return writed_;
}

string_view Reader::peek() const
{
  // Your code here.
  if (buffer_.empty()) {
    return {};
  }
  return {&buffer_.front(), 1};
}

bool Reader::is_finished() const
{
  // Your code here.
  return closed_ and buffer_.empty();
}

bool Reader::has_error() const
{
  // Your code here.
  return error_;
}

void Reader::pop( uint64_t len )
{
  // Your code here.
  if (len > buffer_.size()) {
    len = buffer_.size();
  }
  readed_ += len;
  for (uint64_t i = 0; i < len; i++) {
    buffer_.pop();
  }
}
uint64_t Reader::bytes_buffered() const
{
  // Your code here.
  return buffer_.size();
}

uint64_t Reader::bytes_popped() const
{
  // Your code here.
  return readed_;
}
