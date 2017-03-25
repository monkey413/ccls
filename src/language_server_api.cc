#include "language_server_api.h"

const char* IpcIdToString(IpcId id) {
  switch (id) {
  case IpcId::CancelRequest:
    return "$/cancelRequest";
  case IpcId::Initialize:
    return "initialize";
  case IpcId::Initialized:
    return "initialized";
  case IpcId::TextDocumentDocumentSymbol:
    return "textDocument/documentSymbol";
  case IpcId::TextDocumentCodeLens:
    return "textDocument/codeLens";
  case IpcId::CodeLensResolve:
    return "codeLens/resolve";
  case IpcId::WorkspaceSymbol:
    return "workspace/symbol";
  default:
    assert(false);
    exit(1);
  }
}

void Reflect(Writer& visitor, lsRequestId& value) {
  assert(value.id0.has_value() || value.id1.has_value());

  if (value.id0) {
    Reflect(visitor, value.id0.value());
  }
  else {
    Reflect(visitor, value.id1.value());
  }
}

void Reflect(Reader& visitor, lsRequestId& id) {
  if (visitor.IsInt())
    Reflect(visitor, id.id0);
  else if (visitor.IsString())
    Reflect(visitor, id.id1);
  else
    std::cerr << "Unable to deserialize id" << std::endl;
}

MessageRegistry* MessageRegistry::instance_ = nullptr;

std::unique_ptr<BaseIpcMessage> MessageRegistry::ReadMessageFromStdin() {
  int content_length = -1;
  int iteration = 0;
  while (true) {
    if (++iteration > 10) {
      assert(false && "bad parser state");
      exit(1);
    }

    std::string line;
    std::getline(std::cin, line);
    // std::cin >> line;

    if (line.compare(0, 14, "Content-Length") == 0) {
      content_length = atoi(line.c_str() + 16);
    }

    if (line == "\r")
      break;
  }

  // bad input that is not a message.
  if (content_length < 0) {
    std::cerr << "parsing command failed (no Content-Length header)"
      << std::endl;
    return nullptr;
  }

  std::string content;
  content.reserve(content_length);
  for (int i = 0; i < content_length; ++i) {
    char c;
    std::cin >> c;
    content += c;
  }

  rapidjson::Document document;
  document.Parse(content.c_str(), content_length);
  assert(!document.HasParseError());

  return Parse(document);
}

std::unique_ptr<BaseIpcMessage> MessageRegistry::Parse(Reader& visitor) {
  std::string jsonrpc = visitor["jsonrpc"].GetString();
  if (jsonrpc != "2.0")
    exit(1);

  std::string method;
  ReflectMember(visitor, "method", method);

  if (allocators.find(method) == allocators.end()) {
    std::cerr << "Unable to find registered handler for method \"" << method << "\"" << std::endl;
    return nullptr;
  }

  Allocator& allocator = allocators[method];
  return allocator(visitor);
}

MessageRegistry* MessageRegistry::instance() {
  if (!instance_)
    instance_ = new MessageRegistry();

  return instance_;
}

BaseIpcMessage::BaseIpcMessage(IpcId method_id)
  : method_id(method_id) {}

void lsResponseError::Write(Writer& visitor) {
  auto& value = *this;
  int code = static_cast<int>(this->code);

  visitor.StartObject();
  REFLECT_MEMBER2("code", code);
  REFLECT_MEMBER(message);
  if (data) {
    visitor.Key("data");
    data->Write(visitor);
  }
  visitor.EndObject();
}

lsDocumentUri lsDocumentUri::FromPath(const std::string& path) {
  lsDocumentUri result;
  result.SetPath(path);
  return result;
}

lsDocumentUri::lsDocumentUri() {}

bool lsDocumentUri::operator==(const lsDocumentUri& other) const {
  return raw_uri == other.raw_uri;
}

void lsDocumentUri::SetPath(const std::string& path) {
  // file:///c%3A/Users/jacob/Desktop/superindex/indexer/full_tests
  raw_uri = path;

  size_t index = raw_uri.find(":");
  if (index != -1) {
    raw_uri.replace(raw_uri.begin() + index, raw_uri.begin() + index + 1, "%3A");
  }

  raw_uri = "file:///" + raw_uri;
  //std::cerr << "Set uri to " << raw_uri << " from " << path;
}

std::string lsDocumentUri::GetPath() {
  // TODO: make this not a hack.
  std::string result = raw_uri;

  size_t index = result.find("%3A");
  if (index != -1) {
    result.replace(result.begin() + index, result.begin() + index + 3, ":");
  }

  index = result.find("file://");
  if (index != -1) {
    result.replace(result.begin() + index, result.begin() + index + 8, "");
  }

  std::replace(result.begin(), result.end(), '\\', '/');
  return result;
}

lsPosition::lsPosition() {}
lsPosition::lsPosition(int line, int character) : line(line), character(character) {}

bool lsPosition::operator==(const lsPosition& other) const {
  return line == other.line && character == other.character;
}

lsRange::lsRange() {}
lsRange::lsRange(lsPosition position) : start(position), end(position) {}

bool lsRange::operator==(const lsRange& other) const {
  return start == other.start && end == other.end;
}

lsLocation::lsLocation() {}
lsLocation::lsLocation(lsDocumentUri uri, lsRange range) : uri(uri), range(range) {}

bool lsLocation::operator==(const lsLocation& other) const {
  return uri == other.uri && range == other.range;
}

void Reflect(Reader& reader, lsInitializeParams::lsTrace& value) {
  std::string v = reader.GetString();
  if (v == "off")
    value = lsInitializeParams::lsTrace::Off;
  else if (v == "messages")
    value = lsInitializeParams::lsTrace::Messages;
  else if (v == "verbose")
    value = lsInitializeParams::lsTrace::Verbose;
}

void Reflect(Writer& writer, lsInitializeParams::lsTrace& value) {
  switch (value) {
  case lsInitializeParams::lsTrace::Off:
    writer.String("off");
    break;
  case lsInitializeParams::lsTrace::Messages:
    writer.String("messages");
    break;
  case lsInitializeParams::lsTrace::Verbose:
    writer.String("verbose");
    break;
  }
}

void Reflect(Writer& visitor, lsCodeLensCommandArguments& value) {
  visitor.StartArray();
  Reflect(visitor, value.uri);
  Reflect(visitor, value.position);
  Reflect(visitor, value.locations);
  visitor.EndArray();
}
void Reflect(Reader& visitor, lsCodeLensCommandArguments& value) {
  auto it = visitor.Begin();
  Reflect(*it, value.uri);
  ++it;
  Reflect(*it, value.position);
  ++it;
  Reflect(*it, value.locations);
}

std::string Out_ShowLogMessage::method() {
  if (display_type == DisplayType::Log)
    return "window/logMessage";
  return "window/showMessage";
}