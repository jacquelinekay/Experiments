// Copyright (c) 2015-2016 Vittorio Romeo
// License: Academic Free License ("AFL") v. 3.0
// AFL License page: http://opensource.org/licenses/AFL-3.0
// http://vittorioromeo.info | vittorio.romeo@outlook.com

#pragma once

#include <vrm/sdl/input/null_handlers.hpp>
#include <vrm/sdl/input/kkey.hpp>
#include <vrm/sdl/input/mbtn.hpp>

VRM_SDL_NAMESPACE
{
    namespace impl
    {
        auto& null_key_event_handler() noexcept
        {
            static key_event_handler result([](auto)
                {
                });
            return result;
        }

        auto& null_btn_event_handler() noexcept
        {
            static btn_event_handler result([](auto)
                {
                });
            return result;
        }
    }
}
VRM_SDL_NAMESPACE_END
