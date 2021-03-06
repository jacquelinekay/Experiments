// Copyright (c) 2013-2015 Vittorio Romeo
// License: Academic Free License ("AFL") v. 3.0
// AFL License page: http://opensource.org/licenses/AFL-3.0

#ifndef CESYSTEM_ENTITY
#define CESYSTEM_ENTITY

namespace ssvces
{
    class Entity
    {
        friend Manager;
        friend EntityHandle;
        friend Impl::SystemBase;
        template <typename, typename, typename>
        friend class System;

    private:
        Manager& manager;
        std::array<ComponentRecyclerPtr, maxComponents> components;
        TypeIdxBitset typeIds;
        bool mustDestroy{false}, mustRematch{true};
        GroupBitset groups;
        EntityStat stat;
        SizeT componentCount{0};

    public:
        inline Entity(Manager& mManager, const EntityStat& mStat) noexcept
            : manager(mManager),
              stat(mStat)
        {
        }

        inline Entity(const Entity&) = delete;
        inline Entity& operator=(const Entity&) = delete;

        template <typename T, typename... TArgs>
        inline void createComponent(TArgs&&...);
        template <typename T>
        inline void removeComponent();
        template <typename T>
        inline bool hasComponent() const noexcept
        {
            SSVU_ASSERT_STATIC(ssvu::isBaseOf<Component, T>(),
                "`T` must derive from `Component`");
            return typeIds[Impl::getTypeIdx<T>()];
        }
        template <typename T>
        inline T& getComponent() noexcept
        {
            SSVU_ASSERT_STATIC(ssvu::isBaseOf<Component, T>(),
                "`T` must derive from `Component`");
            SSVU_ASSERT(componentCount > 0 && hasComponent<T>());
            return ssvu::castUp<T>(*components[Impl::getTypeIdx<T>()]);
        }

        inline void destroy() noexcept;

        inline Manager& getManager() noexcept { return manager; }

        // Groups
        inline void setGroups(bool mOn, Group mGroup) noexcept;
        inline void addGroups(Group mGroup) noexcept;
        inline void delGroups(Group mGroup) noexcept;
        inline void clearGroups() noexcept;
        template <typename... TGroups>
        inline void setGroups(
            bool mOn, Group mGroup, TGroups... mGroups) noexcept
        {
            setGroups(mOn, mGroup);
            setGroups(mOn, mGroups...);
        }
        template <typename... TGroups>
        inline void addGroups(Group mGroup, TGroups... mGroups) noexcept
        {
            addGroups(mGroup);
            addGroups(mGroups...);
        }
        template <typename... TGroups>
        inline void delGroups(Group mGroup, TGroups... mGroups) noexcept
        {
            delGroups(mGroup);
            delGroups(mGroups...);
        }
        inline bool hasGroup(Group mGroup) const noexcept
        {
            return groups[mGroup];
        }
        inline bool hasAnyGroup(const GroupBitset& mGroups) const noexcept
        {
            return (groups & mGroups).any();
        }
        inline bool hasAllGroups(const GroupBitset& mGroups) const noexcept
        {
            return (groups & mGroups).all();
        }
        inline const GroupBitset& getGroups() const noexcept { return groups; }
    };

    template <typename T>
    inline constexpr Tpl<T*> buildComponentsTpl(Entity& mEntity)
    {
        return Tpl<T*>{&mEntity.getComponent<T>()};
    }
    template <typename T1, typename T2, typename... TArgs>
    inline constexpr Tpl<T1*, T2*, TArgs*...> buildComponentsTpl(
        Entity& mEntity)
    {
        return ssvu::tplCat(buildComponentsTpl<T1>(mEntity),
            buildComponentsTpl<T2, TArgs...>(mEntity));
    }
}

#endif
