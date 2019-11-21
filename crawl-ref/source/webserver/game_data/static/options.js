define(["jquery", "comm"],
function ($, comm) {
    "use strict";

    var options = null;
    var listeners = $.Callbacks();

    function clear_options()
    {
        options = null;
    }

    function get_option(name)
    {
        if (options == null)
        {
            console.error("Options not set, wanted option: " + name);
            return null;
        }

        if (!options.hasOwnProperty(name))
        {
            console.error("Option doesn't exist: " + name);
            return null;
        }

        return options[name];
    }

    // writes all options at once: usable only on first connecting.
    function handle_options_message(data)
    {
        if (options == null || data["watcher"] == true)
        {
            options = data.options;
            listeners.fire();
        }
    }

    function add_listener(callback)
    {
        listeners.add(callback);
    }

    function set_option(name, value)
    {
        var old = get_option(name);
        console.log(name + ": '" + old + "' => '" + value + "'");
        options[name] = value;
        listeners.fire();
    }

    function handle_set_option(data)
    {
        set_option(data.name, data.value);
    };

    window.set_option = set_option;

    comm.register_handlers({
        "options": handle_options_message,
        "set_option": handle_set_option
    });

    return {
        get: get_option,
        set: set_option,
        clear: clear_options,
        add_listener: add_listener,
    };
});
