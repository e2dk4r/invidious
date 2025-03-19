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
    raw_data = process.ReadMemory(value, length, error)

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
    raw_data = process.ReadMemory(out_buffer_value, length, error)

    if error.Success():
        return f'length: {length} "{raw_data.decode("utf-8", "ignore")}"'
    else:
        return "<error reading memory>"
