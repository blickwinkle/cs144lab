#include "reassembler.hh"

using namespace std;

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring, Writer& output )
{
  // 先把数据插入到buffer中，再合并红黑树节点，最后把可以写入的数据写入到output中
  //如果output已经关闭，那么直接返回
  if (output.is_closed()) {
    return;
  }
  // 如果是最后一个数据包，那么就把eofsize_设置为最后一个数据包的长度
  if (is_last_substring) {
    eofsize_ = first_index + data.length();
  }
  // 如果数据已经被写入到output中，那么取数据没写过的部分
  if (first_index < output.bytes_pushed()) {
    if (first_index + data.length() <= output.bytes_pushed()) {
      return;
    }
    data = data.substr(output.bytes_pushed() - first_index);
    first_index = output.bytes_pushed();
  }
  // 如果数据的大小超过了eofsize_，那么取到eofsize_为止的数据
  if (eofsize_ != UINT64_MAX && first_index + data.length() > eofsize_) {
    data = data.substr(0, eofsize_ - first_index);
  }
  // 如果数据的大小超过了output的可用容量，那么取到output的可用容量为止的数据
  if (first_index + data.length() > output.available_capacity() + output.bytes_pushed()) {
    data = data.substr(0, output.available_capacity() + output.bytes_pushed() - first_index);
  }
  //如果数据已经在buffer中，那么取长度大的数据
  auto it = buffer_.find(first_index);
  if (it != buffer_.end()) {
    if (it->second.length() >= data.length()) {
      return;
    }
    buffer_.erase(it);
  }
  // 把数据插入到buffer中
  buffer_.insert({first_index, data});
  bytes_pending_ = 0;
  // 合并红黑树节点
  it = buffer_.begin();
  while (it != buffer_.end()) {
    auto next = it;
    next++;
    if (next == buffer_.end()) {
      bytes_pending_ += it->second.length();
      break;
    }
    //合并的是重叠的部分
    if (it->first + it->second.length() >= next->first) {
      if (next->first + next->second.length() > it->first + it->second.length()) {
        it->second = it->second.substr(0, next->first - it->first) + next->second;
      }
      buffer_.erase(next);
    } else {
      bytes_pending_ += it->second.length();
      it++;
    }
  }
  // 将可以写入的数据写入到output中
  it = buffer_.begin();
  if (it->first == output.bytes_pushed()) {
    bytes_pending_ -= it->second.length();
    if (it->second.size() > output.available_capacity()) {
      it->second = it->second.substr(0, output.available_capacity());
    }
    output.push(it->second);
    bytes_written_ += it->second.size();
    buffer_.erase(it);
  }
  if (eofsize_ != UINT64_MAX && eofsize_ == bytes_written_) {
    output.close();
  }
}

uint64_t Reassembler::bytes_pending() const
{
  // Your code here.
  return bytes_pending_;
}
