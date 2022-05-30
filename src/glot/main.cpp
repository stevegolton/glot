#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/fwd.hpp>
#include <spdlog/spdlog.h>
#include <database/database.hpp>
#include "plot.hpp"
#include "imgui_database_panel.hpp"
#include "plugin_context.hpp"
#include "plugin_manager.hpp"
#include "window.hpp"
#include <imgui.h>
#include "window_glfw_imgui.hpp"
#include "graph.hpp"
#include "audiofile_plugin.hpp"
#include "wavegen_plugin.hpp"
#include "ui.hpp"

void init_timeseries(Database &database, GraphState &state)
{
    const auto createGlmColour = [](int code) {
        float r = static_cast<float>(0xFF & (code >> 16)) / 256.0f;
        float g = static_cast<float>(0xFF & (code >> 8)) / 256.0f;
        float b = static_cast<float>(0xFF & (code >> 0)) / 256.0f;
        return glm::vec3(r, g, b);
    };

    const auto pick_next_colour = [](auto begin, auto end, auto current) {
        current++;
        if (current == end)
        {
            current = begin;
        }
        return current;
    };

    // A nice selection of material colours from here (column 400):
    // https://material.io/resources/color/
    std::vector<glm::vec3> plot_colours;

    plot_colours.push_back(createGlmColour(0xef5350));
    plot_colours.push_back(createGlmColour(0x42a5f5));
    plot_colours.push_back(createGlmColour(0xd4e157));
    plot_colours.push_back(createGlmColour(0xec407a));
    plot_colours.push_back(createGlmColour(0x26c6da));
    plot_colours.push_back(createGlmColour(0xffee58));
    plot_colours.push_back(createGlmColour(0xab47bc));
    plot_colours.push_back(createGlmColour(0x26a69a));
    plot_colours.push_back(createGlmColour(0xffca28));
    plot_colours.push_back(createGlmColour(0x7e57c2));
    plot_colours.push_back(createGlmColour(0x66bb6a));
    plot_colours.push_back(createGlmColour(0xffa726));
    plot_colours.push_back(createGlmColour(0x5c6bc0));
    plot_colours.push_back(createGlmColour(0x9ccc65));
    plot_colours.push_back(createGlmColour(0xff7043));

    const auto &data = database.data();
    state.timeseries.resize(data.size());
    auto colour = plot_colours.begin();
    std::transform(data.begin(),
                   data.end(),
                   state.timeseries.begin(),
                   [&colour, &plot_colours, &pick_next_colour](const auto &ts) {
                       GraphState::TimeSeriesState cont;
                       cont.name = ts.first;
                       cont.ts = ts.second;
                       cont.colour = *colour;
                       colour = pick_next_colour(plot_colours.begin(), plot_colours.end(), colour);
                       cont.visible = true;
                       cont.y_offset = 0.0f;
                       return cont;
                   });
}

class KeyController : public View
{
  public:
    KeyController(Window &window, Graph &graph) : m_window(window), m_graph(graph)
    {
        //
    }

    void on_key(Window &, int key, int, int action, int mods) override
    {
        if (action == GLFW_PRESS)
        {
            if (key == GLFW_KEY_F11 && action == GLFW_PRESS)
            {
                m_window.set_fullscreen(!m_window.is_fullscreen());
            }
            if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
            {
                m_window.request_close();
            }
            if (key == GLFW_KEY_SPACE && action == GLFW_PRESS)
            {
                m_graph.reveal_newest_sample();
            }
            if (key == GLFW_KEY_A && action == GLFW_PRESS)
            {
                if (mods == GLFW_MOD_CONTROL)
                {
                    m_graph.set_marker_visible(Graph::MarkerType::A, false);
                }
                else
                {
                    const auto cursor_gs = m_graph.cursor_gs();
                    m_graph.set_marker_visible(Graph::MarkerType::A, true);
                    m_graph.set_marker_position(Graph::MarkerType::A, cursor_gs.x);
                }
            }
            else if (key == GLFW_KEY_B && action == GLFW_PRESS)
            {
                if (mods == GLFW_MOD_CONTROL)
                {
                    m_graph.set_marker_visible(Graph::MarkerType::B, false);
                }
                else
                {
                    const auto cursor_gs = m_graph.cursor_gs();
                    m_graph.set_marker_visible(Graph::MarkerType::B, true);
                    m_graph.set_marker_position(Graph::MarkerType::B, cursor_gs.x);
                }
            }
            else if (key == GLFW_KEY_C && action == GLFW_PRESS)
            {
                m_graph.set_marker_visible(Graph::MarkerType::A, false);
                m_graph.set_marker_visible(Graph::MarkerType::B, false);
            }
        }
    }

  private:
    Window &m_window;
    Graph &m_graph;
};

int main()
{
    spdlog::info("Initializing...");

    try
    {
        // Create the timeseries database - this is where all the data goes!
        Database db;

        // Create and launch a selection of example plugins
        PluginContext plugin_context(db);
        PluginManager plugin_manager;
        plugin_manager.add_plugin(
            "audiofile",
            std::make_shared<AudioFilePlugin>(plugin_context, "audio/Lurking_Threat_3.wav"));
        plugin_manager.add_plugin("wavegen", std::make_shared<WaveGenPlugin>(plugin_context));
        plugin_manager.start_all();

        // Create a new window using GLFW, OpenGL and initializing ImGui
        Window_GLFW_ImGui window(1024, 768, "GLot");

        // We need to do this after creating our GL context which is done when the first GLFW
        // window is created
        if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
        {
            throw std::runtime_error("Failed to initialize GLAD");
        }

        GraphState state;
        init_timeseries(db, state);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        Graph graph(state, window);
        window.add_view(&graph);

        KeyController key_controller(window, graph);
        window.add_view(&key_controller);

        ImGuiMenuView gui(window, plugin_manager, graph, db, state);
        window.add_imgui_view(&gui);

        spdlog::info("Initialization OK, starting main loop");

        window.init();

        // Main loop
        while (!window.should_close())
        {
            glfwPollEvents();
            window.render();
        }
    }
    catch (const std::exception &e)
    {
        spdlog::error("Error: {}", e.what());
    }

    glfwTerminate();
    spdlog::info("Shutting down normally");
    return 0;
}
