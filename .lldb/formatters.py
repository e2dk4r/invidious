# see https://lldb.llvm.org/use/variable.html#synthetic-children for documentation
import lldb

# include/text.h
def string_summary(valobj, dict):
    value = valobj.GetChildMemberWithName("value").GetValueAsUnsigned(0)
    length = valobj.GetChildMemberWithName("length").GetValueAsUnsigned(0)

    if value == 0:
        return "(null string)"
    if value != 0 and length == 0:
        return "(empty string)"

    process = valobj.GetProcess()
    error = lldb.SBError()
    readLength = min(length, 20)
    raw_data = process.ReadMemory(value, readLength, error)

    if error.Success():
        return f'(length: {length}) "{raw_data.decode("utf-8", "ignore")}"'
    else:
        return "<error reading memory>"

# include/string_builder.h
def string_builder_summary(valobj, dict):
    length = valobj.GetChildMemberWithName("length").GetValueAsUnsigned(0)
    out_buffer_value = valobj.GetChildMemberWithName("outBuffer").GetChildMemberWithName("value").GetValueAsUnsigned(0)

    if length == 0 or out_buffer_value == 0:
        return "length: 0"

    process = valobj.GetProcess()
    error = lldb.SBError()
    readLength = min(length, 20)
    raw_data = process.ReadMemory(out_buffer_value, readLength, error)

    if error.Success():
        return f'length: {length} "{raw_data.decode("utf-8", "ignore")}"'
    else:
        return "<error reading memory>"

# include/string_cursor.h
def string_cursor_summary(valobj, dict):
    position = valobj.GetChildMemberWithName("position").GetValueAsUnsigned(0)
    source = valobj.GetChildMemberWithName("source")
    value = source.GetChildMemberWithName("value").GetValueAsUnsigned(0)
    length = source.GetChildMemberWithName("length").GetValueAsUnsigned(0)

    if value == 0:
        return "(null string)"
    if value != 0 and length == 0:
        return "(empty string)"

    process = valobj.GetProcess()
    error = lldb.SBError()
    readLength = min(length - position, 20)
    raw_data = process.ReadMemory(value + position, readLength, error)

    if error.Success():
        return f'(position: {position}, remaining: {length - position}) "{raw_data.decode("utf-8", "ignore")}"'
    else:
        return "<error reading memory>"


def __lldb_init_module(debugger, internal_dict):
    debugger.HandleCommand('type summary add string --python-function formatters.string_summary')
    debugger.HandleCommand('type summary add string_builder --python-function formatters.string_builder_summary')
    debugger.HandleCommand('type summary add string_cursor --python-function formatters.string_cursor_summary')
