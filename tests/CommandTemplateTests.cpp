#include "core/CommandTemplate.hpp"

#include <cassert>
#include <iostream>

using namespace altrun;

int main() {
    {
        Command command;
        command.target =
            L"code";
        command.arguments =
            L"\"{folder}\" --reuse-window";
        command.workingDirectory =
            L"{folder}";

        assert(
            UsesFolderTemplate(
                command));

        const auto resolved =
            ResolveFolderTemplate(
                command,
                L"D:\\Research\\CKD");

        assert(
            resolved.target ==
            L"code");
        assert(
            resolved.arguments ==
            L"\"D:\\Research\\CKD\" --reuse-window");
        assert(
            resolved.workingDirectory ==
            L"D:\\Research\\CKD");
    }

    {
        Command command;
        command.target =
            L"{folder}\\tools\\run.cmd";
        command.arguments =
            L"--root={folder} --again={folder}";

        const auto resolved =
            ResolveFolderTemplate(
                command,
                L"\\\\server\\share\\\u9879\u76ee");

        assert(
            resolved.target ==
            L"\\\\server\\share\\\u9879\u76ee\\tools\\run.cmd");
        assert(
            resolved.arguments ==
            L"--root=\\\\server\\share\\\u9879\u76ee "
            L"--again=\\\\server\\share\\\u9879\u76ee");
    }

    {
        Command command;
        command.target =
            L"notepad.exe";

        assert(
            !UsesFolderTemplate(
                command));
        assert(
            ResolveFolderTemplate(
                command,
                L"D:\\Ignored")
                .target ==
            command.target);
    }

    std::cout
        << "Command template tests passed\n";
    return 0;
}
