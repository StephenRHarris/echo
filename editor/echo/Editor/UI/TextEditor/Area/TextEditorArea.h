#pragma once

#include <QMainWindow>
#include "LuaEditor.h"
#include "ui_TextEditorArea.h"

namespace Studio
{
	class TextEditorArea : public QDockWidget, public Ui_TextEditorArea
	{
		Q_OBJECT

	public:
		TextEditorArea(QWidget* parent = 0);
		~TextEditorArea();

		// open lua file
		void open(const Echo::String& fullPath, bool isRememberOpenStates=true);
        
        // get tab index
        bool getTabIndex(const Echo::String& fullPath, int& index);
        
        // remember|recover script open states
        void rememberScriptOpenStates();
        void recoverScriptOpenStates();

	protected slots:
		// save
		void save();
        
        // on tab index changed
        void onTabIdxChanged(int idx);
        
        // on lua editor title changed
        void onLuaEditorTitleChanged(LuaEditor* editor);

	private:
		Echo::vector<LuaEditor*>::type	m_luaEditors;
	};
}
