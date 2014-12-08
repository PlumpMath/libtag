#include "nametagGroup.h"

#include <asyncTaskManager.h>
#include <boundingBox.h>
#include <boundingHexahedron.h>

static PT(AsyncTaskManager) g_task_mgr = AsyncTaskManager::get_global_ptr(); 

TypeHandle NametagGroup::_type_handle;
unsigned int NametagGroup::NametagGroup_serial;

NametagGroup::NametagGroup() : m_nametag_2d(new Nametag2d()), m_nametag_3d(new Nametag3d()),
                               m_icon(new PandaNode("icon")), m_chat_timeout_task(NULL),
                               m_stomp_flags(0), m_color_code(NametagConstants::CC_normal),
                               m_chat_flags(0), m_font(NULL), m_avatar(NULL), m_manager(NULL),
                               m_qt_color(LVecBase4f(1)), m_active(true), m_chat_page(0),
                               m_visible_3d(true), m_stomp_task(NULL)                               
{
    add_nametag(m_nametag_2d);
    add_nametag(m_nametag_3d);
    
    m_serial = NametagGroup::NametagGroup_serial++;
    
    m_tick_task = new GenericAsyncTask(get_unique_id(), &NametagGroup::tick_task, (void*)this);
    m_tick_task->set_sort(45);
    g_task_mgr->add(m_tick_task);
}

NametagGroup::~NametagGroup()
{
    delete m_icon;
    delete m_nametag_2d;
    delete m_nametag_3d;
    delete m_tick_task;
    delete m_stomp_task;
    delete m_chat_timeout_task;
}

void NametagGroup::destroy()
{
    if (m_tick_task != NULL)
    {
        m_tick_task->remove();
        m_tick_task = NULL;
    }
    
    if (m_stomp_task != NULL)
    {
        m_stomp_task->remove();
        m_stomp_task = NULL;
    }
    
    if (m_manager != NULL)
        unmanage(m_manager);
    
    for (nametag_vec_t::iterator it = m_nametags.begin(); it != m_nametags.end(); ++it)
        remove_nametag(*it);
        
    m_nametags.clear();
}

Nametag2d* NametagGroup::get_nametag_2d()
{
    return m_nametag_2d;
}

Nametag3d* NametagGroup::get_nametag_3d()
{
    return m_nametag_3d;
}

PT(PandaNode) NametagGroup::get_name_icon()
{
    return m_icon;
}

unsigned int NametagGroup::get_num_chat_pages()
{
    if (!(m_chat_flags & (NametagConstants::CF_speech | NametagConstants::CF_thought)))
        return 0;
        
    return m_chat_pages.size();
}

void NametagGroup::set_page_number(int page)
{
    m_chat_page = page;
    update_tags();
}

bool NametagGroup::get_chat_stomp()
{
    return m_stomp_task != NULL;
}

const std::wstring NametagGroup::get_chat()
{
    std::wstring empty;
    if (m_chat_page >= m_chat_pages.size())
        return empty;
        
    return m_chat_pages.at(m_chat_page);
}

const std::wstring NametagGroup::get_stomp_text()
{
    return m_stomp_text;
}

float NametagGroup::get_stomp_delay()
{
    return .2;
};

const std::string NametagGroup::get_unique_id()
{
    std::string s = "Nametag-";
    s += m_serial;
    return s;
}

bool NametagGroup::has_button()
{
    return get_buttons().size() > 0;
}

void NametagGroup::set_active(bool active)
{
    m_active = active;
}

bool NametagGroup::is_active()
{
    return m_active;
}

void NametagGroup::set_avatar(NodePath* avatar)
{
    m_avatar = avatar;
}

void NametagGroup::set_font(PT(TextFont) font)
{
    m_font = font;
    update_tags();
}

void NametagGroup::set_color_code(NametagConstants::ColorCode cc)
{
    m_color_code = cc;
    update_tags();
}

void NametagGroup::set_name(const std::wstring& name)
{
    m_name = name;
    update_tags();
}

void NametagGroup::set_display_name(const std::wstring& name)
{
    m_display_name = name;
    update_tags();
}

void NametagGroup::set_qt_color(LVecBase4f color)
{
    m_qt_color = color;
    update_tags();
}

void NametagGroup::set_chat(const std::wstring& chat_string, int chat_flags)
{
    if (!(m_chat_flags & NametagConstants::CF_speech))
        update_chat(chat_string, chat_flags);
        
    else
    {
        clear_chat();
        m_stomp_text = chat_string;
        m_stomp_flags = chat_flags;
        m_stomp_task = new GenericAsyncTask(get_unique_id(), &NametagGroup::update_stomp_task, (void*)this);
        m_stomp_task->set_delay(get_stomp_delay());
        g_task_mgr->add(m_stomp_task);
    }
}

void NametagGroup::set_contents(int contents)
{
    for (nametag_vec_t::iterator it = m_nametags.begin(); it != m_nametags.end(); ++it)
        (*it)->set_contents(contents);
}

void NametagGroup::clear_shadow()
{
} // No op

void NametagGroup::clear_chat()
{
    const std::wstring empty;
    update_chat(empty, 0);
    if (m_stomp_task != NULL)
    {
        m_stomp_task->remove();
        m_stomp_task = NULL;
    }
}

void NametagGroup::add_nametag(Nametag* nametag)
{
    m_nametags.push_back(nametag);
    update_nametag(nametag);
    
    if (m_manager != NULL && nametag->is_of_type(MarginPopup::get_class_type()))
        DCAST(MarginPopup, nametag)->manage(m_manager);
}

void NametagGroup::remove_nametag(Nametag* nametag)
{
    nametag_vec_t::iterator it = std::find(m_nametags.begin(), m_nametags.end(), nametag);
    if (it != m_nametags.end())
    {
        m_nametags.erase(it);
        
        if (m_manager != NULL && nametag->is_of_type(MarginPopup::get_class_type()))
            DCAST(MarginPopup, nametag)->unmanage(m_manager);
            
        nametag->destroy();
    }
}

void NametagGroup::manage(MarginManager* manager)
{
    m_manager = manager;
    for (nametag_vec_t::iterator it = m_nametags.begin(); it != m_nametags.end(); ++it)
    {
        Nametag* nametag = *it;
        if (nametag->is_of_type(MarginPopup::get_class_type()))
            DCAST(MarginPopup, nametag)->manage(manager);
    }
}

void NametagGroup::unmanage(MarginManager* manager)
{
    m_manager = NULL;
    for (nametag_vec_t::iterator it = m_nametags.begin(); it != m_nametags.end(); ++it)
    {
        Nametag* nametag = *it;
        if (nametag->is_of_type(MarginPopup::get_class_type()))
            DCAST(MarginPopup, nametag)->unmanage(manager);
    }
}

void NametagGroup::set_name_wordwrap(float wordwrap)
{
} // No op

buttons_map_t NametagGroup::get_buttons()
{
    buttons_map_t empty;
    
    if (get_num_chat_pages() < 2)
    {
        if (m_chat_flags & NametagConstants::CF_page_button)
            return NametagGlobals::page_buttons;
           
        else if (m_chat_flags & NametagConstants::CF_quit_button)
            return NametagGlobals::quit_buttons;
            
        else
            return empty;
    }
    
    else if (m_chat_page == get_num_chat_pages() - 1)
        return (m_chat_flags & NametagConstants::CF_no_quit_button) ? empty : NametagGlobals::quit_buttons;
        
    return (m_chat_flags & NametagConstants::CF_page_button) ? NametagGlobals::page_buttons : empty;
    
}

void NametagGroup::update_nametag(Nametag* nametag)
{
    nametag->m_font = m_font;
    nametag->m_name = m_name;
    nametag->m_display_name = m_display_name;
    nametag->m_qt_color = m_qt_color;
    nametag->m_color_code = m_color_code;
    nametag->m_chat_string = get_chat();
    nametag->m_buttons = get_buttons();
    nametag->m_chat_flags = m_chat_flags;
    nametag->m_avatar = m_avatar;
    nametag->m_icon = NodePath(m_icon);
    
    if (m_active || has_button())
        nametag->set_click_region_event(get_unique_id());
        
    else
        nametag->set_click_region_event("");
        
    nametag->update();
}

void NametagGroup::update_tags()
{
    for (nametag_vec_t::iterator it = m_nametags.begin(); it != m_nametags.end(); ++it)
        update_nametag(*it);
}

void NametagGroup::update_chat(const std::wstring& chat_string, int chat_flags)
{
    m_chat_pages.clear();
    m_chat_flags = 0;
    
    if (chat_string.size())
    {
        m_chat_flags = chat_flags;
        
        // Split the string
        const wchar_t* begin;
        const wchar_t* str = chat_string.c_str();
        
        do
        {
            begin = str;

            while(*str != '\x07' && *str)
                str++;

            m_chat_pages.push_back(std::wstring(begin, str));
        }
        while (*str++ != NULL);
    }
    
    set_page_number(0);
    
    stop_chat_timeout();
    if (chat_flags & NametagConstants::CF_timeout)
        start_chat_timeout();
}

void NametagGroup::start_chat_timeout()
{
    float timeout = get_chat().size() / 2.;
    timeout = timeout < 4 ? 4 : timeout;
    timeout = timeout > 12 ? 12 : timeout;
    
    m_chat_timeout_task = new GenericAsyncTask(get_unique_id(), &NametagGroup::do_chat_timeout_task, (void*)this);
    m_chat_timeout_task->set_delay(timeout);
    g_task_mgr->add(m_chat_timeout_task);
}

void NametagGroup::stop_chat_timeout()
{
    if (m_chat_timeout_task != NULL)
    {
        m_chat_timeout_task->remove();
        m_chat_timeout_task = NULL;
    }
}

AsyncTask::DoneStatus NametagGroup::update_stomp()
{
    update_chat(m_stomp_text, m_stomp_flags);
    m_stomp_task = NULL;
    return AsyncTask::DS_done;
}

AsyncTask::DoneStatus NametagGroup::do_chat_timeout()
{
    const std::wstring empty;
    update_chat(empty, 0);
    return AsyncTask::DS_done;
}

AsyncTask::DoneStatus NametagGroup::tick()
{
    for (nametag_vec_t::iterator it = m_nametags.begin(); it != m_nametags.end(); ++it)
        (*it)->tick();
        
    if (m_avatar == NULL)
        return AsyncTask::DS_cont;
        
    else if (m_avatar->is_empty())
        return AsyncTask::DS_cont;
            
    bool visible_3d;
    
    if (m_avatar->is_hidden())
        visible_3d = false;
        
    else if (NametagGlobals::onscreen_chat_forced && m_chat_flags & NametagConstants::CF_speech)
        visible_3d = false;
        
    else
    {
        LPoint3f min_corner, max_corner;
        m_avatar->calc_tight_bounds(min_corner, max_corner);
        PT(BoundingBox) avatar_bounds = new BoundingBox(min_corner, max_corner);
        PT(BoundingHexahedron) camera_bounds = DCAST(BoundingHexahedron, DCAST(Camera, NametagGlobals::camera.node())->get_lens()->make_bounds());
        camera_bounds->xform(NametagGlobals::camera.get_mat(m_avatar->get_parent()));
        visible_3d = (camera_bounds->contains(avatar_bounds) & BoundingVolume::IF_some);
    }
    
    if (visible_3d != m_visible_3d)
    {
        m_visible_3d = visible_3d;
        for (nametag_vec_t::iterator it = m_nametags.begin(); it != m_nametags.end(); ++it)
        {
            Nametag* nametag = *it;
            nametag->tick();
            if (nametag->is_of_type(MarginPopup::get_class_type()))
                DCAST(MarginPopup, nametag)->set_visible(!visible_3d);
        }
    }
    
    return AsyncTask::DS_cont;
}

AsyncTask::DoneStatus NametagGroup::update_stomp_task(GenericAsyncTask* task, void* data)
{
    return ((NametagGroup*)data)->update_stomp();
}

AsyncTask::DoneStatus NametagGroup::do_chat_timeout_task(GenericAsyncTask* task, void* data)
{
    return ((NametagGroup*)data)->do_chat_timeout();
}

AsyncTask::DoneStatus NametagGroup::tick_task(GenericAsyncTask* task, void* data)
{
    return ((NametagGroup*)data)->tick();
}
