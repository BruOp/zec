#include "app.h"

class HelloWorldApp : public zec::App
{
public:
    HelloWorldApp() : App{ L"Hello World!" } { }

protected:
    void init() override final
    { }

    void shutdown() override final
    { }

    void update(const zec::TimeData& time_data) override final
    { }

    void render() override final
    { }

    void before_reset() override final
    { }

    void after_reset() override final
    { }
};

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
    HelloWorldApp app{};
    return app.run();
}