#include "log.hpp"
#include "filesystem.hpp"
#include "assertion.hpp"
#include <regex>

static quill::Logger* CreateDefaultLogger()
{
    quill::Config QuillConfig;
    QuillConfig.enable_console_colours = true;
    QuillConfig.backend_thread_empty_all_queues_before_exit = true;
    quill::configure(QuillConfig);

    quill::start(true);

    std::string LogPattern = "[%(ascii_time)] [%(thread_name)] [%(filename):%(lineno)] [%(level_name)] - %(message)";
    std::string TimeFormat = "%H:%M:%S.%Qms";

    quill::FileHandlerConfig FileConfig{};
    FileConfig.set_open_mode('w');
    FileConfig.set_pattern(LogPattern, TimeFormat);
    FileConfig.set_append_to_filename(quill::FilenameAppend::StartDateTime);

    quill::FileEventNotifier FileEvent{};
    FileEvent.before_write = [](std::string_view Message) -> std::string //remove all colors from the message
    {
        return libassert::utility::strip_colors(std::string{Message});
    };

    std::string LogFile = ProjectRelativePath("saved/logs/log.txt");

    std::shared_ptr<quill::Handler> file_handler = quill::file_handler(LogFile, FileConfig, FileEvent);

    std::shared_ptr<quill::Handler> stdout_handler = quill::stdout_handler();
    stdout_handler->set_pattern(LogPattern, TimeFormat);

    quill::Logger* Logger = quill::create_logger("default logger", {stdout_handler, file_handler});

    QUILL_LOG_INFO(Logger, "created logger");
    QUILL_LOG_INFO(Logger, "logging to {}", dynamic_cast<quill::FileHandler*>(file_handler.get())->filename());

    return Logger;
}

quill::Logger* GetQuillLogger()
{
    static quill::Logger* Logger = CreateDefaultLogger();
    return Logger;
}
