// Copyright (c) 2015-2016 Vittorio Romeo
// License: Academic Free License ("AFL") v. 3.0
// AFL License page: http://opensource.org/licenses/AFL-3.0
// http://vittorioromeo.info | vittorio.romeo@outlook.com

#pragma once

#include <chrono>
#include <thread>
#include <vrm/sdl/math.hpp>
#include <vrm/sdl/common.hpp>
#include <vrm/sdl/resource.hpp>
#include <vrm/sdl/elements.hpp>
#include <vrm/sdl/context/context.hpp>

namespace vrm
{
    namespace sdl
    {
        namespace impl
        {

            template <typename TSettings>
            auto& interpolated_engine<TSettings>::update_fn() noexcept
            {
                return _update_fn;
            }

            template <typename TSettings>
            auto& interpolated_engine<TSettings>::draw_fn() noexcept
            {
                return _draw_fn;
            }

            template <typename TSettings>
            auto& interpolated_engine<TSettings>::interpolate_fn() noexcept
            {
                return _interpolate_fn;
            }

            template <typename TSettings>
            void interpolated_engine<TSettings>::run_update(ft step)
            {
                _prev_state = _current_state;
                this->update_fn()(_current_state, step);
            }

            template <typename TSettings>
            void interpolated_engine<TSettings>::run_draw()
            {
                this->_interpolate_fn(_interpolated_state, _prev_state,
                    _current_state, _timer->interp_t());

                draw_fn()(_interpolated_state);
            }



            template <typename TSettings>
            auto& non_interpolated_engine<TSettings>::update_fn() noexcept
            {
                return _update_fn;
            }

            template <typename TSettings>
            auto& non_interpolated_engine<TSettings>::draw_fn() noexcept
            {
                return _draw_fn;
            }

            template <typename TSettings>
            auto& non_interpolated_engine<TSettings>::interpolate_fn() noexcept
            {
                return _interpolate_fn;
            }

            template <typename TSettings>
            void non_interpolated_engine<TSettings>::run_update(ft step)
            {
                this->update_fn()(_current_state, step);
            }

            template <typename TSettings>
            void non_interpolated_engine<TSettings>::run_draw()
            {
                draw_fn()(_current_state);
            }



            template <typename TSettings>
            auto& context<TSettings>::on_key_up() noexcept
            {
                return _on_key_up;
            }

            template <typename TSettings>
            auto& context<TSettings>::on_key_down() noexcept
            {
                return _on_key_down;
            }

            template <typename TSettings>
            auto& context<TSettings>::on_btn_up() noexcept
            {
                return _on_btn_up;
            }

            template <typename TSettings>
            auto& context<TSettings>::on_btn_down() noexcept
            {
                return _on_btn_down;
            }

            template <typename TSettings>
            void context<TSettings>::run_events()
            {
                while(SDL_PollEvent(&_event))
                {
                    switch(_event.type)
                    {
                        case SDL_KEYDOWN:
                            on_key_down()(
                                static_cast<kkey>(_event.key.keysym.scancode));
                            break;

                        case SDL_KEYUP:
                            on_key_up()(
                                static_cast<kkey>(_event.key.keysym.scancode));
                            break;

                        case SDL_MOUSEBUTTONDOWN:
                            on_btn_down()(
                                static_cast<mbtn>(_event.button.button));
                            break;

                        case SDL_MOUSEBUTTONUP:
                            on_btn_up()(
                                static_cast<mbtn>(_event.button.button));
                            break;

                        case SDL_MOUSEMOTION:
                            _input_state.mouse_x(_event.motion.x);
                            _input_state.mouse_y(_event.motion.y);
                            break;

                        case SDL_QUIT: std::terminate(); break;
                        case SDL_WINDOWEVENT: break;
                        case SDL_FINGERDOWN: break;
                        case SDL_FINGERUP: break;
                        case SDL_FINGERMOTION: break;
                    }
                }
            }



            template <typename TSettings>
            context<TSettings>::context(
                const std::string& title, std::size_t width, std::size_t height)
                : _width{width}, _height{height}, _window{title, width, height},
                  _glcontext{*_window}
            {
                if(TTF_Init() != 0)
                {
                    log_sdl_error("ttf_init");
                    std::terminate();
                }

                on_key_down() = [this](auto k)
                {
                    _input_state.key(k, true);
                };

                on_key_up() = [this](auto k)
                {
                    _input_state.key(k, false);
                };

                on_btn_down() = [this](auto b)
                {
                    _input_state.btn(b, true);
                };

                on_btn_up() = [this](auto b)
                {
                    _input_state.btn(b, false);
                };
            }

            template <typename TSettings>
            const auto& context<TSettings>::update_duration() const noexcept
            {
                return _update_duration;
            }

            template <typename TSettings>
            const auto& context<TSettings>::draw_duration() const noexcept
            {
                return _draw_duration;
            }

            template <typename TSettings>
            const auto& context<TSettings>::total_duration() const noexcept
            {
                return _total_duration;
            }

            template <typename TSettings>
            const auto& context<TSettings>::real_duration() const noexcept
            {
                return _real_duration;
            }


            template <typename TSettings>
            template <typename T>
            auto context<TSettings>::ms_from_duration(const T& duration) const
                noexcept
            {
                return std::chrono::duration_cast<ms_double_duration>(duration)
                    .count();
            }

            template <typename TSettings>
            auto context<TSettings>::update_ms() const noexcept
            {
                return ms_from_duration(update_duration());
            }

            template <typename TSettings>
            auto context<TSettings>::draw_ms() const noexcept
            {
                return ms_from_duration(draw_duration());
            }

            template <typename TSettings>
            auto context<TSettings>::total_ms() const noexcept
            {
                return ms_from_duration(total_duration());
            }

            template <typename TSettings>
            auto context<TSettings>::real_ms() const noexcept
            {
                return ms_from_duration(real_duration());
            }

            template <typename TSettings>
            auto& context<TSettings>::timer() noexcept
            {
                return *_engine->_timer;
            }

            template <typename TSettings>
            const auto& context<TSettings>::timer() const noexcept
            {
                return *_engine->_timer;
            }

            template <typename TSettings>
            void context<TSettings>::run()
            {
                auto time_dur([](auto&& f)
                    {
                        auto ms_start(hr_clock::now());
                        f();

                        return hr_clock::now() - ms_start;
                    });

                _real_duration = time_dur([&, this]
                    {
                        _total_duration = time_dur([&, this]
                            {
                                run_events();

                                _update_duration = time_dur([&, this]
                                    {
                                        timer().run(real_ms(),
                                            [&, this](auto step)
                                            {
                                                _engine->run_update(step);
                                            });
                                    });

                                _draw_duration = time_dur([this]
                                    {
                                        glClear(GL_COLOR_BUFFER_BIT |
                                                GL_DEPTH_BUFFER_BIT |
                                                GL_STENCIL_BUFFER_BIT);

                                        _engine->run_draw();

                                        SDL_GL_SwapWindow(*_window);
                                    });
                            });

                        limit_fps_if_necessary();
                    });
            }

            template <typename TSettings>
            void context<TSettings>::limit_fps_if_necessary() const noexcept
            {
                if(total_ms() >= ms_limit()) return;

                auto delay_ms(ms_limit() - total_ms());
                SDL_Delay(std::round(delay_ms));
            }

            template <typename TSettings>
            auto context<TSettings>::fps() const noexcept
            {
                // constexpr float seconds_ft_ratio{60.f};
                // return seconds_ft_ratio / total_ms();
                // return total_duration().count();

                return static_cast<int>(1000.f / real_ms());
            }

            template <typename TSettings>
            auto context<TSettings>::mouse_x() const noexcept
            {
                return _input_state.mouse_x();
            }

            template <typename TSettings>
            auto context<TSettings>::mouse_y() const noexcept
            {
                return _input_state.mouse_y();
            }

            template <typename TSettings>
            auto context<TSettings>::mouse_pos() const noexcept
            {
                return _input_state.mouse_pos();
            }

            template <typename TSettings>
            auto context<TSettings>::key(kkey k) const noexcept
            {
                return _input_state.key(k);
            }

            template <typename TSettings>
            auto context<TSettings>::btn(mbtn b) const noexcept
            {
                return _input_state.btn(b);
            }

            template <typename TSettings>
            template <typename... Ts>
            auto context<TSettings>::make_surface(Ts&&... xs) noexcept
            {
                return unique_surface(FWD(xs)...);
            }

            template <typename TSettings>
            void context<TSettings>::title(const std::string& s) noexcept
            {
                this->_window->title(s);
            }
        }
    }
}