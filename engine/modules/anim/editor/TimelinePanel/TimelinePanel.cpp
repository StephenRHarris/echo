#include "TimelinePanel.h"
#include "engine/core/editor/editor.h"
#include "engine/core/editor/qt/QWidgets.h"
#include "engine/core/base/class_method_bind.h"
#include "engine/core/util/PathUtil.h"
#include "engine/core/util/StringUtil.h"
#include "../../anim_timeline.h"

namespace Echo
{
#ifdef ECHO_EDITOR_MODE
	class TimelinePanelUtil
	{
	public:
		// is a object item
		static bool isObject(QTreeWidgetItem* item)
		{
			String text = EditorApi.qTreeWidgetItemText(item, 0);
			String userData = EditorApi.qTreeWidgetItemUserData(item, 0);
			return userData == "object" ? true : false;
		}

		// is a property item
		static bool isProperty(QTreeWidgetItem* item)
		{
			String text = EditorApi.qTreeWidgetItemText(item, 0);
			String userData = EditorApi.qTreeWidgetItemUserData(item, 0);
			return userData == "property" ? true : false;
		}

		// get property chain and object path
		static void queryNodePathAndPropertyChain(QTreeWidgetItem* item, String& nodePath, StringArray& propertyChain)
		{
			String userData = EditorApi.qTreeWidgetItemUserData(item, 0);
			if (userData == "property")
			{
				QTreeWidgetItem* nodeItem = EditorApi.qTreeWidgetItemParent(item);
				propertyChain.insert(propertyChain.begin(), EditorApi.qTreeWidgetItemText(item, 0));
				while (isProperty(nodeItem))
				{
					propertyChain.insert(propertyChain.begin(), EditorApi.qTreeWidgetItemText(nodeItem, 0));
					nodeItem = EditorApi.qTreeWidgetItemParent(nodeItem);
				}

				nodePath = EditorApi.qTreeWidgetItemText(nodeItem, 0);
			}
			else
			{
				propertyChain.clear();
				nodePath = EditorApi.qTreeWidgetItemText(item, 0);
			}
		}

		// get value
		static bool getProertyValue(Timeline* timeline, String& objectPath, const StringArray& propertyChain, Variant& oVar)
		{
			Object* lastObject = timeline->getLastObject(objectPath, propertyChain);
			if (lastObject)
			{
				Class::getPropertyValue(lastObject, propertyChain.back(), oVar);
				return true;
			}

			return false;
		}
	};

	TimelinePanel::TimelinePanel(Object* obj)
		: m_addObjectMenu(nullptr)
		, m_nodeTreeWidgetWidth(0)
		, m_rulerHeight( 25.f)
		, m_rulerColor( 0.73f, 0.73f, 0.73f)
		, m_curveKeyLineEdit(nullptr)
		, m_curveKeyItem(nullptr)
	{
		m_curveItems.assign(nullptr);
		m_curveVisibles.assign(true);

		m_timeline = ECHO_DOWN_CAST<Timeline*>(obj);

		m_ui = EditorApi.qLoadUi("engine/modules/anim/editor/TimelinePanel/TimelinePanel.ui");

		QWidget* splitter = EditorApi.qFindChild(m_ui, "m_splitter");
		if (splitter)
		{
			EditorApi.qSplitterSetStretchFactor(splitter, 0, 0);
			EditorApi.qSplitterSetStretchFactor(splitter, 1, 1);
		}

		// Top tool buttons icons
		EditorApi.qToolButtonSetIcon(EditorApi.qFindChild(m_ui, "AddNode"), "engine/modules/anim/editor/icon/add.png");
		EditorApi.qToolButtonSetIcon(EditorApi.qFindChild(m_ui, "Play"), "engine/modules/anim/editor/icon/play.png");
		EditorApi.qToolButtonSetIcon(EditorApi.qFindChild(m_ui, "Stop"), "engine/modules/anim/editor/icon/stop.png");
		EditorApi.qToolButtonSetIcon(EditorApi.qFindChild(m_ui, "Restart"), "engine/modules/anim/editor/icon/replay.png");

		// set fixed width of add toolbutton
		EditorApi.qToolButtonSetIconSize(EditorApi.qFindChild(m_ui, "AddNode"), 24, 24);

		// set toolbuttons icons
		EditorApi.qToolButtonSetIcon( EditorApi.qFindChild(m_ui, "NewClip"), "engine/modules/anim/editor/icon/new.png");
		EditorApi.qToolButtonSetIcon( EditorApi.qFindChild(m_ui, "DuplicateClip"), "engine/modules/anim/editor/icon/duplicate.png");
		EditorApi.qToolButtonSetIcon( EditorApi.qFindChild(m_ui, "DeleteClip"), "engine/modules/anim/editor/icon/delete.png");

		// set curve toolbutton icons
		EditorApi.qToolButtonSetIcon(EditorApi.qFindChild(m_ui, "m_curveXVisible"), "engine/modules/anim/editor/icon/curve_red.png");
		EditorApi.qToolButtonSetIcon(EditorApi.qFindChild(m_ui, "m_curveYVisible"), "engine/modules/anim/editor/icon/curve_green.png");
		EditorApi.qToolButtonSetIcon(EditorApi.qFindChild(m_ui, "m_curveZVisible"), "engine/modules/anim/editor/icon/curve_blue.png");
		EditorApi.qToolButtonSetIcon(EditorApi.qFindChild(m_ui, "m_curveWVisible"), "engine/modules/anim/editor/icon/curve_white.png");

		// connect signal slots
		EditorApi.qConnectWidget(EditorApi.qFindChild(m_ui, "NewClip"), QSIGNAL(clicked()), this, createMethodBind(&TimelinePanel::onNewClip));
		EditorApi.qConnectWidget(EditorApi.qFindChild(m_ui, "DuplicateClip"), QSIGNAL(clicked()), this, createMethodBind(&TimelinePanel::onDuplicateClip));
		EditorApi.qConnectWidget(EditorApi.qFindChild(m_ui, "DeleteClip"), QSIGNAL(clicked()), this, createMethodBind(&TimelinePanel::onDeleteClip));
		EditorApi.qConnectWidget(EditorApi.qFindChild(m_ui, "m_clipLengthLineEdit"), QSIGNAL(editingFinished()), this, createMethodBind(&TimelinePanel::onCurrentEditAnimLengthChanged));
		EditorApi.qConnectWidget(EditorApi.qFindChild(m_ui, "m_clips"), QSIGNAL(editTextChanged(const QString &)), this, createMethodBind(&TimelinePanel::onRenameClip));
		EditorApi.qConnectWidget(EditorApi.qFindChild(m_ui, "m_clips"), QSIGNAL(currentIndexChanged(int)), this, createMethodBind(&TimelinePanel::onCurrentEditAnimChanged));
		EditorApi.qConnectWidget(EditorApi.qFindChild(m_ui, "AddNode"), QSIGNAL(clicked()), this, createMethodBind(&TimelinePanel::onAddObject));
		EditorApi.qConnectWidget(EditorApi.qFindChild(m_ui, "m_nodeTreeWidget"), QSIGNAL(itemClicked(QTreeWidgetItem*, int)), this, createMethodBind(&TimelinePanel::onSelectItem));
		EditorApi.qConnectWidget(EditorApi.qFindChild(m_ui, "Play"), QSIGNAL(clicked()), this, createMethodBind(&TimelinePanel::onPlayAnim));
		EditorApi.qConnectWidget(EditorApi.qFindChild(m_ui, "Stop"), QSIGNAL(clicked()), this, createMethodBind(&TimelinePanel::onStopAnim));
		EditorApi.qConnectWidget(EditorApi.qFindChild(m_ui, "Restart"), QSIGNAL(clicked()), this, createMethodBind(&TimelinePanel::onRestartAnim));

		EditorApi.qConnectWidget(EditorApi.qFindChild(m_ui, "m_curveXVisible"), QSIGNAL(clicked()), this, createMethodBind(&TimelinePanel::onSwitchCurveVisibility));
		EditorApi.qConnectWidget(EditorApi.qFindChild(m_ui, "m_curveYVisible"), QSIGNAL(clicked()), this, createMethodBind(&TimelinePanel::onSwitchCurveVisibility));
		EditorApi.qConnectWidget(EditorApi.qFindChild(m_ui, "m_curveZVisible"), QSIGNAL(clicked()), this, createMethodBind(&TimelinePanel::onSwitchCurveVisibility));
		EditorApi.qConnectWidget(EditorApi.qFindChild(m_ui, "m_curveWVisible"), QSIGNAL(clicked()), this, createMethodBind(&TimelinePanel::onSwitchCurveVisibility));

		// connect signals
		EditorApi.qConnectWidget(EditorApi.qFindChild(m_ui, "m_graphicsView"), QSIGNAL(customContextMenuRequested(const QPoint&)), this, createMethodBind(&TimelinePanel::onRightClickGraphicsView));

		// create QGraphicsScene
		m_graphicsScene = (QGraphicsScene*)EditorApi.qGraphicsSceneNew();
		EditorApi.qGraphicsViewSetScene(EditorApi.qFindChild(m_ui, "m_graphicsView"), m_graphicsScene);

		// wheel event
		EditorApi.qConnectObject( m_graphicsScene, QSIGNAL(wheelEvent(QGraphicsSceneWheelEvent*)), this, createMethodBind(&TimelinePanel::onGraphicsSceneWheelEvent));

		// draw ruler
		switchCurveEditor();
	}

	void TimelinePanel::syncDataToEditor()
	{
		// update display
		syncClipListDataToEditor();
	}

	void TimelinePanel::onNewClip()
	{
		if (m_timeline)
		{
			AnimClip* animClip = EchoNew(AnimClip);
			animClip->m_name = getNewClipName();
			m_timeline->addClip(animClip);

			syncClipListDataToEditor();

			// set current edit anim clip
			setCurrentEditAnim(animClip->m_name.c_str());
		}
	}

	void TimelinePanel::update()
	{
		onNodeTreeWidgetSizeChanged();

		// update ruler draw
		drawRuler();
	}

	void TimelinePanel::onDuplicateClip()
	{
		String currentAnim = EditorApi.qComboBoxCurrentText(EditorApi.qFindChild(m_ui, "m_clips"));
		if (!currentAnim.empty() && m_timeline)
		{
			AnimClip* animClip = m_timeline->getClip(currentAnim.c_str());
			AnimClip* animClipDuplicate = animClip->duplicate();
			m_timeline->generateUniqueAnimName(currentAnim + " Duplicate ", animClipDuplicate->m_name);
			m_timeline->addClip(animClipDuplicate);

			syncClipListDataToEditor();

			// set current edit anim clip
			setCurrentEditAnim(animClipDuplicate->m_name.c_str());
		}
	}

	void TimelinePanel::onDeleteClip()
	{
		String currentAnim = EditorApi.qComboBoxCurrentText(EditorApi.qFindChild(m_ui, "m_clips"));
		if (!currentAnim.empty() && m_timeline)
		{
			m_timeline->deleteClip(currentAnim.c_str());

			syncClipListDataToEditor();

			if (m_timeline->getClipCount() > 0)
			{
				setCurrentEditAnim(m_timeline->getClip(0)->m_name.c_str());
			}
		}
	}

	void TimelinePanel::onRenameClip()
	{
		String currentAnimName = EditorApi.qComboBoxCurrentText(EditorApi.qFindChild(m_ui, "m_clips"));
		if (!currentAnimName.empty() && m_timeline)
		{
			int currentIndex = EditorApi.qComboBoxCurrentIndex(EditorApi.qFindChild(m_ui, "m_clips"));
			m_timeline->renameClip(currentIndex, currentAnimName.c_str());

			EditorApi.qComboBoxSetItemText(EditorApi.qFindChild(m_ui, "m_clips"), currentIndex, currentAnimName.c_str());
			setCurrentEditAnim(currentAnimName.c_str());
		}
	}

	void TimelinePanel::onAddObject()
	{
		if (!m_addObjectMenu)
		{
			m_addObjectMenu = EditorApi.qMenuNew(m_ui);

			EditorApi.qMenuAddAction(m_addObjectMenu, EditorApi.qFindChildAction(m_ui, "m_actionAddNode"));
			EditorApi.qMenuAddAction(m_addObjectMenu, EditorApi.qFindChildAction(m_ui, "m_actionAddSetting"));
			EditorApi.qMenuAddAction(m_addObjectMenu, EditorApi.qFindChildAction(m_ui, "m_actionAddResource"));

			EditorApi.qConnectAction(EditorApi.qFindChildAction(m_ui, "m_actionAddNode"), QSIGNAL(triggered()), this, createMethodBind(&TimelinePanel::onAddNode));
			EditorApi.qConnectAction(EditorApi.qFindChildAction(m_ui, "m_actionAddSetting"), QSIGNAL(triggered()), this, createMethodBind(&TimelinePanel::onAddSetting));
			EditorApi.qConnectAction(EditorApi.qFindChildAction(m_ui, "m_actionAddResource"), QSIGNAL(triggered()), this, createMethodBind(&TimelinePanel::onAddResource));
		}

		EditorApi.qMenuExec(m_addObjectMenu);
	}

	void TimelinePanel::onAddNode()
	{
		Echo::String path = Editor::instance()->selectANodeObject();
		if (!path.empty())
		{
			Node* node = m_timeline->getNode(path.c_str());
			if (node)
			{
				String relativePath = node->getNodePathRelativeTo(m_timeline);
				m_timeline->addObject(m_currentEditAnim, Timeline::Node, relativePath);
			}

			syncClipNodeDataToEditor();
		}
	}

	void TimelinePanel::onAddSetting()
	{
		Echo::String path = Editor::instance()->selectASettingObject();
		if (!path.empty())
		{
			m_timeline->addObject(m_currentEditAnim, Timeline::Setting, path);

			syncClipNodeDataToEditor();
		}
	}

	void TimelinePanel::onAddResource()
	{
		Echo::String path = Editor::instance()->selectAResObject("");
		if (!path.empty())
		{
			m_timeline->addObject(m_currentEditAnim, Timeline::Resource, path);

			syncClipNodeDataToEditor();
		}
	}

	String TimelinePanel::getNewClipName()
	{
		if (m_timeline)
		{
			Echo::String newName;
			m_timeline->generateUniqueAnimName("Anim ", newName);
			return newName;
		}

		return StringUtil::BLANK;
	}

	void TimelinePanel::syncClipTimeLength()
	{
		AnimClip* clip = m_timeline->getClip(m_currentEditAnim.c_str());
		EditorApi.qLineEditSetText( EditorApi.qFindChild(m_ui, "m_clipLengthLineEdit"), clip ? StringUtil::Format("%d", clip->m_length) : StringUtil::BLANK);
	}

	void TimelinePanel::syncClipListDataToEditor()
	{
		if (m_timeline)
		{
			QWidget* comboBox = EditorApi.qFindChild(m_ui, "m_clips");
			if (comboBox)
			{
				EditorApi.qComboBoxClear(comboBox);

				const StringOption& anims = m_timeline->getAnim();
				for (const String& animName : anims.getOptions())
				{
					EditorApi.qComboBoxAddItem(comboBox, nullptr, animName.c_str());
				}
			}
		}
	}

	void TimelinePanel::syncClipNodeDataToEditor()
	{
		QWidget* nodeTreeWidget = EditorApi.qFindChild(m_ui, "m_nodeTreeWidget");
		if (nodeTreeWidget)
		{
			EditorApi.qTreeWidgetClear(nodeTreeWidget);

			AnimClip* clip = m_timeline->getClip(m_currentEditAnim.c_str());
			if (clip)
			{
				QTreeWidgetItem* rootItem = EditorApi.qTreeWidgetInvisibleRootItem(nodeTreeWidget);
				if (rootItem)
				{
					for (AnimObject* animNode : clip->m_objects)
					{
						const Timeline::ObjectUserData& userData = any_cast<Timeline::ObjectUserData>(animNode->m_userData);
						Node* node = m_timeline->getNode(userData.m_path.c_str());
						QTreeWidgetItem* objetcItem = EditorApi.qTreeWidgetItemNew();
						EditorApi.qTreeWidgetItemSetText(objetcItem, 0, userData.m_path.c_str());
						EditorApi.qTreeWidgetItemSetUserData(objetcItem, 0, "object");
						EditorApi.qTreeWidgetItemSetUserData(objetcItem, 1, "object");
						//EditorApi.qTreeWidgetItemSetIcon(objetcItem, 0, Editor::instance()->getNodeIcon(node).c_str());
						EditorApi.qTreeWidgetItemSetIcon(objetcItem, 1, "engine/modules/anim/editor/icon/add.png");
						EditorApi.qTreeWidgetItemAddChild(rootItem, objetcItem);

						for (AnimProperty* property : animNode->m_properties)
						{
							syncPropertyDataToEditor(objetcItem, property, StringUtil::Split(property->m_name));
						}
					}
				}
			}
		}
	}

	void TimelinePanel::syncPropertyDataToEditor(QTreeWidgetItem* parentItem, AnimProperty* property, const StringArray& propertyChain)
	{
		if (propertyChain.size() > 0)
		{
			String propertyName = propertyChain.front();
			QTreeWidgetItem* propertyItem = nullptr;

			// is exist
			for (i32 i = 0; i < EditorApi.qTreeWidgetItemChildCount(parentItem); i++)
			{
				QTreeWidgetItem* child = EditorApi.qTreeWidgetItemChild(parentItem, i);
				if (child && EditorApi.qTreeWidgetItemText(child, 0)==propertyName)
				{
					propertyItem = child;
				}
			}

			// create new
			if (!propertyItem)
			{
				propertyItem = EditorApi.qTreeWidgetItemNew();
				EditorApi.qTreeWidgetItemSetText(propertyItem, 0, propertyName.c_str());
				EditorApi.qTreeWidgetItemSetUserData(propertyItem, 0, "property");
				EditorApi.qTreeWidgetItemSetExpanded(parentItem, true);
				EditorApi.qTreeWidgetItemAddChild(parentItem, propertyItem);

				if (property->getType() == AnimProperty::Type::Object)
				{
					EditorApi.qTreeWidgetItemSetUserData(propertyItem, 1, "property");
					EditorApi.qTreeWidgetItemSetIcon(propertyItem, 1, "engine/modules/anim/editor/icon/add.png");
				}
			}

			// add last property
			if (propertyChain.size() > 1)
			{
				StringArray lastPropertyChain = propertyChain;
				lastPropertyChain.erase(lastPropertyChain.begin());
				syncPropertyDataToEditor(propertyItem, property, lastPropertyChain);
			}
		}
	}

	void TimelinePanel::addNodePropertyToEditor()
	{

	}

	void TimelinePanel::setCurrentEditAnim(const char* animName)
	{
		QWidget* comboBox = EditorApi.qFindChild(m_ui, "m_clips");
		if (comboBox)
		{
			int index = m_timeline->getClipIndex(animName);
			EditorApi.qComboBoxSetCurrentIndex( comboBox, index);

			m_currentEditAnim = animName;

			// sync clip node data to editor
			syncClipNodeDataToEditor();

			// sync clip time length data to editor
			syncClipTimeLength();
		}
	}

	void TimelinePanel::onSelectItem()
	{
		//QTreeWidgetItem* item = qTreeWidgetCurrentItem(qFindChild(m_ui, "m_nodeTreeWidget"));
		int column = EditorApi.qTreeWidgetCurrentColumn(EditorApi.qFindChild(m_ui, "m_nodeTreeWidget"));
		if (column == 1)
		{
			onAddProperty();
		}
		else if (column == 0)
		{
			onSelectProperty();
		}
	}

	void TimelinePanel::onAddProperty()
	{
		QTreeWidgetItem* item = EditorApi.qTreeWidgetCurrentItem(EditorApi.qFindChild(m_ui, "m_nodeTreeWidget"));
		int column = EditorApi.qTreeWidgetCurrentColumn(EditorApi.qFindChild(m_ui, "m_nodeTreeWidget"));
		if (column == 1)
		{
			String userData = EditorApi.qTreeWidgetItemUserData(item, column);
			Echo::StringArray propertyChain;
			if (userData == "object")
			{
				String text = EditorApi.qTreeWidgetItemText(item, 0);
				Node* node = m_timeline->getNode(text.c_str());
				if (node)
				{
					String propertyName = Editor::instance()->selectAProperty(node);
					if (!propertyName.empty())
					{
						propertyChain.insert(propertyChain.begin(), propertyName);
						AnimProperty::Type propertyType = m_timeline->getAnimPropertyType(node->getNodePathRelativeTo(m_timeline), propertyChain);
						if (propertyType != AnimProperty::Type::Unknown)
						{
							m_timeline->addProperty(m_currentEditAnim, node->getNodePathRelativeTo(m_timeline), propertyChain, propertyType);

							// addNodePropertyToEditor;
							QTreeWidgetItem* propertyItem = EditorApi.qTreeWidgetItemNew();
							EditorApi.qTreeWidgetItemSetText(propertyItem, 0, propertyName.c_str());
							EditorApi.qTreeWidgetItemSetUserData(propertyItem, 0, "property");
							EditorApi.qTreeWidgetItemSetExpanded(item, true);
							EditorApi.qTreeWidgetItemAddChild(item, propertyItem);

							if (propertyType == AnimProperty::Type::Object)
							{
								EditorApi.qTreeWidgetItemSetUserData(propertyItem, 1, "property");
								EditorApi.qTreeWidgetItemSetIcon(propertyItem, 1, "engine/modules/anim/editor/icon/add.png");
							}
						}
					}
				}
			}
			else if (userData == "property")
			{
				QTreeWidgetItem* nodeItem = EditorApi.qTreeWidgetItemParent(item);
				propertyChain.insert(propertyChain.begin(), EditorApi.qTreeWidgetItemText(item, 0));
				while (TimelinePanelUtil::isProperty(nodeItem))
				{
					nodeItem = EditorApi.qTreeWidgetItemParent(nodeItem);
					propertyChain.insert(propertyChain.begin(), EditorApi.qTreeWidgetItemText(nodeItem, 0));
				}

				String text = EditorApi.qTreeWidgetItemText(nodeItem, 0);
				Node* node = m_timeline->getNode(text.c_str());
				Object* propertyObject = node;
				for (Echo::String propertyName : propertyChain)
				{
					Echo::Variant propertyValue;
					Class::getPropertyValue(propertyObject, propertyName, propertyValue);

					propertyObject = propertyValue.toObj();
				}

				String propertyName = Editor::instance()->selectAProperty(propertyObject);
				if (!propertyName.empty())
				{
					propertyChain.emplace_back(propertyName);

					AnimProperty::Type propertyType = m_timeline->getAnimPropertyType(node->getNodePathRelativeTo(m_timeline), propertyChain);
					if (propertyType != AnimProperty::Type::Unknown)
					{
						if(m_timeline->addProperty(m_currentEditAnim, node->getNodePathRelativeTo(m_timeline), propertyChain, propertyType))
						{ 
							// addNodePropertyToEditor;
							QTreeWidgetItem* propertyItem = EditorApi.qTreeWidgetItemNew();
							EditorApi.qTreeWidgetItemSetText(propertyItem, 0, propertyName.c_str());
							EditorApi.qTreeWidgetItemSetUserData(propertyItem, 0, "property");
							EditorApi.qTreeWidgetItemSetExpanded(item, true);
							EditorApi.qTreeWidgetItemAddChild(item, propertyItem);

							if (propertyType == AnimProperty::Type::Object)
							{
								EditorApi.qTreeWidgetItemSetUserData(propertyItem, 1, "property");
								EditorApi.qTreeWidgetItemSetIcon(propertyItem, 1, "engine/modules/anim/editor/icon/add.png");
							}
						}
					}
				}
			}
		}
	}

	void TimelinePanel::onSelectProperty()
	{
		QTreeWidgetItem* item = EditorApi.qTreeWidgetCurrentItem(EditorApi.qFindChild(m_ui, "m_nodeTreeWidget"));
		int column = EditorApi.qTreeWidgetCurrentColumn(EditorApi.qFindChild(m_ui, "m_nodeTreeWidget"));
		if (column == 0)
		{
			String userData = EditorApi.qTreeWidgetItemUserData( item, column);
			if (userData == "property")
			{
				TimelinePanelUtil::queryNodePathAndPropertyChain(item, m_currentEditObjectPath, m_currentEditPropertyChain);
			}
			else
			{
				m_currentEditPropertyChain.clear();
			}

			// refresh curve display
			refreshCurveDisplayToEditor(m_currentEditObjectPath, m_currentEditPropertyChain);

			// refresh curve key display
			refreshCurveKeyDisplayToEditor(m_currentEditObjectPath, m_currentEditPropertyChain);

			// enable disable curve edit
			switchCurveEditor();
			
			// zoom curve
			zoomCurve();
		}
	}

	void TimelinePanel::clearCurveItemsTo(int number)
	{
		//for (size_t i = 0; i < m_curveItems.size(); i++)
		//{
		//	if (!m_curveItems[i])
		//	{
		//		vector<Vector2>::type paths;
		//		m_curveItems[i] = qGraphicsSceneAddPath(m_graphicsScene, paths);
		//	}
		//}
	}

	void TimelinePanel::refreshCurveDisplayToEditor(const String& objectPath, const StringArray& propertyChain)
	{
		// clear curves
		for (size_t i = 0; i < m_curveItems.size(); i++)
		{
			if (m_curveItems[i])
			{
				EditorApi.qGraphicsSceneDeleteItem(m_graphicsScene, m_curveItems[i]);
				m_curveItems[i] = nullptr;
			}
		}

		// recreate curves
		AnimProperty* animProperty = m_timeline->getProperty(m_currentEditAnim, objectPath, propertyChain);
		if (animProperty)
		{
			switch (animProperty->m_type)
			{
			case AnimProperty::Type::Bool:
				{
					Vector2 keyPos;
					calcKeyPosByTimeAndValue( animProperty->getLength(), 0.f, keyPos);
					m_curveItems[0] = EditorApi.qGraphicsSceneAddLine(m_graphicsScene, 0.f, 0.f, keyPos.x, 0.f, Color(1.f, 0.f, 0.f, 0.7f));
				}
				break;
			case AnimProperty::Type::Vector3:
			{
				AnimPropertyVec3* vec3Proeprty = ECHO_DOWN_CAST<AnimPropertyVec3*>(animProperty);
				if (vec3Proeprty)
				{
					const ui32 frameStep = 2;

					//clearCurveItemsTo(3);
					vector<Vector2>::type curvePaths[3];
					for (i32 curveIdx = 0; curveIdx < 3; curveIdx++)
					{
						AnimCurve* curve = vec3Proeprty->m_curves[curveIdx];
						if (curve && curve->getKeyCount())
						{
							for (ui32 t = curve->getStartTime(); t <= curve->getEndTime(); t += frameStep)
							{
								float value = curve->getValue(t);

								Vector2 keyPos;
								calcKeyPosByTimeAndValue(t, value, keyPos);
								curvePaths[curveIdx].emplace_back(keyPos);
							}
						}
					}

					m_curveItems[0] = EditorApi.qGraphicsSceneAddPath(m_graphicsScene, curvePaths[0], 2.5f, Color( 1.f, 0.f, 0.f, 0.7f));
					m_curveItems[1] = EditorApi.qGraphicsSceneAddPath(m_graphicsScene, curvePaths[1], 2.5f, Color( 0.f, 1.f, 0.f, 0.7f));
					m_curveItems[2] = EditorApi.qGraphicsSceneAddPath(m_graphicsScene, curvePaths[2], 2.5f, Color( 0.f, 0.f, 1.f, 0.7f));
				}
			}
			break;
            default: break;
			}
		}
	}

	void TimelinePanel::refreshCurveKeyDisplayToEditor(const String& objectPath, const StringArray& propertyChain)
	{
		// clear all key items
		for (size_t i = 0; i < m_curveItems.size(); i++)
		{
			for (QGraphicsItem* item : m_curveKeyItems[i])
			{
				EditorApi.qGraphicsSceneDeleteItem(m_graphicsScene, item);
			}

			m_curveKeyItems[i].clear();
		}

		for (QGraphicsProxyWidget* widget : m_curveKeyWidgets)
		{
			EditorApi.qGraphicsSceneDeleteWidget(m_graphicsScene, widget);
		}
		m_curveKeyWidgets.clear();

		// create key
		AnimProperty* animProperty = m_timeline->getProperty(m_currentEditAnim, objectPath, propertyChain);
		if (animProperty)
		{
			array<Color, 4> KeyColors = { Color::RED, Color::GREEN, Color::BLUE, Color::WHITE};

			switch (animProperty->m_type)
			{
			case AnimProperty::Type::Bool:
				{
					AnimPropertyBool* boolProperty = ECHO_DOWN_CAST<AnimPropertyBool*>(animProperty);
					if (boolProperty)
					{
						//i32 keyIdx = 0;
						for (auto& it : boolProperty->m_keys)
						{
							ui32 t = it.first;
							float value = it.second;

							Vector2 center;
							calcKeyPosByTimeAndValue(t, value, center);

							QWidget* checkBox = EditorApi.qCheckBoxNew();
							QGraphicsProxyWidget* widget = EditorApi.qGraphicsSceneAddWidget(m_graphicsScene, checkBox);

							EditorApi.qGraphicsProxyWidgetSetPos(widget, center.x, 0.f);
							EditorApi.qGraphicsProxyWidgetSetZValue(widget, 250.f);

							// set userdata
							//String userData = StringUtil::Format("%s,%s,%s,%d,%d", m_currentEditAnim.c_str(), objectPath.c_str(), propertyName.c_str(), 0, keyIdx++);
							//qGraphicsItemSetUserData(item, userData.c_str());

							// set tooltip
							//String toolTip = StringUtil::Format("Time : %d\nValue: %.3f", t, value);
							//qGraphicsItemSetToolTip(item, toolTip.c_str());

							// set moveable
							//qGraphicsItemSetMoveable(item, true);

							// connect signal slots
							//qConnect(item, QSIGNAL(mouseDoubleClickEvent(QGraphicsSceneMouseEvent*)), this, createMethodBind(&TimelinePanel::onKeyDoubleClickedCurveKey));
							//qConnect(item, QSIGNAL(mouseMoveEvent(QGraphicsSceneMouseEvent*)), this, createMethodBind(&TimelinePanel::onKeyPositionChanged));

							m_curveKeyWidgets.emplace_back( widget);
						}
					}
				}
				break;
			case AnimProperty::Type::Vector3:
			{
				AnimPropertyVec3* vec3Proeprty = ECHO_DOWN_CAST<AnimPropertyVec3*>(animProperty);
				if (vec3Proeprty)
				{
					for (int curveIdx = 0; curveIdx < 3; curveIdx++)
					{
						AnimCurve* curve = vec3Proeprty->m_curves[curveIdx];
						for (int keyIdx = 0; keyIdx < curve->getKeyCount(); keyIdx++)
						{
							ui32 t = curve->getKeyTime(keyIdx);
							float value = curve->getValueByKeyIdx(keyIdx);

							Vector2 center;
							calcKeyPosByTimeAndValue(t, value, center);

							QGraphicsItem* item = EditorApi.qGraphicsSceneAddEclipse(m_graphicsScene, center.x - m_keyRadius, center.y - m_keyRadius, m_keyRadius * 2.f, m_keyRadius*2.f, KeyColors[curveIdx]);

							// set user data
							String userData = StringUtil::Format("%s,%s,%s,%d,%d", m_currentEditAnim.c_str(), objectPath.c_str(), StringUtil::ToString(propertyChain).c_str(), curveIdx, keyIdx);
							EditorApi.qGraphicsItemSetUserData(item, userData.c_str());

							// set tooltip
							String toolTip = StringUtil::Format("Time : %d\nValue: %.3f", t, value);
							EditorApi.qGraphicsItemSetToolTip(item, toolTip.c_str());

							// set move able
							EditorApi.qGraphicsItemSetMoveable(item, true);

							// connect signal slots
							EditorApi.qConnectGraphicsItem(item, QSIGNAL(mouseDoubleClickEvent(QGraphicsSceneMouseEvent*)), this, createMethodBind(&TimelinePanel::onKeyDoubleClickedCurveKey));
							EditorApi.qConnectGraphicsItem(item, QSIGNAL(mouseMoveEvent(QGraphicsSceneMouseEvent*)), this, createMethodBind(&TimelinePanel::onKeyPositionChanged));

							m_curveKeyItems[curveIdx].emplace_back(item);
						}
					}
				}
			}
			break;
			case AnimProperty::Type::String:
			{
				AnimPropertyString* strProperty = ECHO_DOWN_CAST<AnimPropertyString*>(animProperty);
				if (strProperty)
				{
					//i32 keyIdx = 0;
					for (auto& it : strProperty->m_keys)
					{
						ui32 t = it.first;
						String value = it.second;

						Vector2 center;
						calcKeyPosByTimeAndValue(t, 0.f, center);

						QWidget* checkBox = EditorApi.qCheckBoxNew();
						QGraphicsProxyWidget* widget = EditorApi.qGraphicsSceneAddWidget(m_graphicsScene, checkBox);

						EditorApi.qGraphicsProxyWidgetSetPos(widget, center.x, 50.f);
						EditorApi.qGraphicsProxyWidgetSetZValue(widget, 250.f);

						m_curveKeyWidgets.emplace_back(widget);
					}
				}
			}
			break;
                    
            default: break;
			}
		}
	}

	bool TimelinePanel::getKeyInfo(TimelinePanel::KeyInfo& keyInfo, const String& animName, const String& objectPath, const String& propertyName, int curveIdx, int keyIdx)
	{
		AnimProperty* animProperty = m_timeline->getProperty(m_currentEditAnim, objectPath, StringUtil::Split(propertyName));
		if (animProperty)
		{
			switch (animProperty->m_type)
			{
			case AnimProperty::Type::Vector3:
			{
				AnimPropertyVec3* vec3Property = ECHO_DOWN_CAST<AnimPropertyVec3*>(animProperty);
				if (vec3Property)
				{
					AnimCurve* curve = vec3Property->m_curves[curveIdx];

					keyInfo.m_type = KeyInfo::Type::Float;
					keyInfo.m_value = curve->getValueByKeyIdx(keyIdx);

					return true;
				}
			}
			break;
                    
            default: break;
			}
		}

		return false;
	}

	void TimelinePanel::onKeyDoubleClickedCurveKey()
	{
		// show key value editor widget
		if (!m_curveKeyLineEdit)
		{
			m_curveKeyLineEdit = EditorApi.qLineEditNew();
			EditorApi.qLineEditSetMaximumWidth(m_curveKeyLineEdit, 100);
			m_curveKeyLineEditProxyWidget = EditorApi.qGraphicsSceneAddWidget(m_graphicsScene, m_curveKeyLineEdit);

			EditorApi.qConnectWidget(m_curveKeyLineEdit, QSIGNAL(editingFinished()), this, createMethodBind(&TimelinePanel::onCurveKeyEditingFinished));
		}

		// get current key value
		QGraphicsItem* sender = EditorApi.qSenderItem();
		if (sender)
		{
			String userData = EditorApi.qGraphicsItemUserData(sender);
			StringArray userDataSplits = StringUtil::Split(userData, ",");
			KeyInfo keyInfo;
			if (getKeyInfo(keyInfo, userDataSplits[0], userDataSplits[1], userDataSplits[2], StringUtil::ParseI32(userDataSplits[3]), StringUtil::ParseI32(userDataSplits[4])))
			{
				m_curveKeyItem = sender;
				EditorApi.qLineEditSetText( m_curveKeyLineEdit, StringUtil::ToString( keyInfo.m_value));

				//int halfWidth = EditorApi.qLineEditWidth(m_curveKeyLineEdit) / 2;
				int halfHeight = EditorApi.qLineEditHeight(m_curveKeyLineEdit) / 2;
				Vector2 mouseScenePos = EditorApi.qGraphicsItemGetEventAll(sender).graphicsSceneMouseEvent.scenePos;
				EditorApi.qGraphicsProxyWidgetSetPos(m_curveKeyLineEditProxyWidget, mouseScenePos.x + m_keyRadius, mouseScenePos.y - halfHeight);
				EditorApi.qGraphicsProxyWidgetSetZValue(m_curveKeyLineEditProxyWidget, 250.f);
			}
		}

		EditorApi.qWidgetSetVisible(m_curveKeyLineEdit, true);
		EditorApi.qLineEditSetCursorPosition(m_curveKeyLineEdit, 0);
	}

	void TimelinePanel::onCurveKeyEditingFinished()
	{
		String valueStr = EditorApi.qLineEditText(m_curveKeyLineEdit);
		if (!valueStr.empty() && m_curveKeyItem)
		{
			String userData = EditorApi.qGraphicsItemUserData(m_curveKeyItem);
			StringArray userDataSplits = StringUtil::Split(userData, ",");

			// modify key value
			float value = StringUtil::ParseFloat(valueStr);
			m_timeline->setKey(userDataSplits[0], userDataSplits[1], userDataSplits[2], StringUtil::ParseI32(userDataSplits[3]), StringUtil::ParseI32(userDataSplits[4]), value);
		}

		EditorApi.qWidgetSetVisible( m_curveKeyLineEdit, false);

		// refresh curve and key display
		refreshCurveDisplayToEditor(m_currentEditObjectPath, m_currentEditPropertyChain);
		refreshCurveKeyDisplayToEditor(m_currentEditObjectPath, m_currentEditPropertyChain);
	}

	void TimelinePanel::onKeyPositionChanged()
	{
		QGraphicsItem* sender = EditorApi.qSenderItem();
		if (sender)
		{
			Vector2 scenePos = EditorApi.qGraphicsItemPos(sender);
			i32 time;
			float value;
			calcKeyTimeAndValueByPos(scenePos, time, value);

			// modify tooltip
			String toolTip = StringUtil::Format("Time : %d\nValue: %.3f", time, value);
			EditorApi.qGraphicsItemSetToolTip(sender, toolTip.c_str());
		}
	}

	void TimelinePanel::onPlayAnim()
	{
		if (m_timeline->getPlayState() != Timeline::PlayState::Playing)
		{
			m_timeline->play(m_currentEditAnim.c_str());

			EditorApi.qToolButtonSetIcon(EditorApi.qFindChild(m_ui, "Play"), "engine/modules/anim/editor/icon/pause.png");
		}
		else
		{
			m_timeline->pause();

			EditorApi.qToolButtonSetIcon(EditorApi.qFindChild(m_ui, "Play"), "engine/modules/anim/editor/icon/play.png");
		}
	}

	void TimelinePanel::onStopAnim()
	{
		m_timeline->stop();

		// recover play button icon to "play.png"
		EditorApi.qToolButtonSetIcon(EditorApi.qFindChild(m_ui, "Play"), "engine/modules/anim/editor/icon/play.png");
	}

	void TimelinePanel::onRestartAnim()
	{
		onStopAnim();
		onPlayAnim();
	}

	void TimelinePanel::onNodeTreeWidgetSizeChanged()
	{
		QWidget* nodeTreeWidget = EditorApi.qFindChild(m_ui, "m_nodeTreeWidget");
		if (nodeTreeWidget)
		{
			int curWidth = EditorApi.qTreeWidgetWidth(nodeTreeWidget);
			if (m_nodeTreeWidgetWidth != curWidth)
			{
				QWidget* header = EditorApi.qTreeWidgetHeader(nodeTreeWidget);

				EditorApi.qHeaderViewResizeSection(header, 1, 30);
				EditorApi.qHeaderViewSetSectionResizeMode(header, 1, QHeaderViewResizeMode::Fixed);
				EditorApi.qHeaderViewResizeSection(header, 0, curWidth - 30);
				m_nodeTreeWidgetWidth = EditorApi.qTreeWidgetWidth(nodeTreeWidget);
			}
		}
	}

	void TimelinePanel::onCurrentEditAnimChanged()
	{
		String currentText = EditorApi.qComboBoxCurrentText(EditorApi.qFindChild(m_ui, "m_clips"));
		setCurrentEditAnim( currentText.c_str());

		onStopAnim();
	}

	void TimelinePanel::onCurrentEditAnimLengthChanged()
	{
		if (!m_currentEditAnim.empty())
		{
			AnimClip* clip = m_timeline->getClip(m_currentEditAnim.c_str());
			if (clip)
			{
				ui32 newLength = StringUtil::ParseUI32( EditorApi.qLineEditText(EditorApi.qFindChild(m_ui, "m_clipLengthLineEdit")), clip->m_length);
				clip->setLength(newLength);
			}
		}
	}

	void TimelinePanel::drawRuler()
	{
		// test view port rect
		Rect viewRect;
		EditorApi.qGraphicsViewSceneRect(EditorApi.qFindChild(m_ui, "m_graphicsView"), viewRect);
		if (viewRect.left != m_rulerLeft || viewRect.top != m_rulerTop)
		{
			m_rulerLeft = int(viewRect.left / 20) * 20;
			m_rulerTop = viewRect.top;

			for (QGraphicsItem* item : m_rulerItems)
			{
				EditorApi.qGraphicsSceneDeleteItem(m_graphicsScene, item);
			}
			m_rulerItems.clear();

			const float keyWidth = 20;	// pixels
			const int   keyCount = 100;

			// ruler bottom
			Color bgColor; bgColor.setRGBA(83, 83, 83, 255);
			m_rulerItems.emplace_back(EditorApi.qGraphicsSceneAddRect(m_graphicsScene, std::max<float>(float(-keyWidth) + m_rulerLeft, -keyWidth), -1 + m_rulerTop, float(keyCount * keyWidth) + keyWidth, m_rulerHeight, bgColor, bgColor));
			m_rulerItems.emplace_back(EditorApi.qGraphicsSceneAddLine(m_graphicsScene, std::max<float>(float(-keyWidth) + m_rulerLeft, 0), m_rulerHeight + m_rulerTop, float(keyCount * keyWidth) + keyWidth + m_rulerLeft, m_rulerHeight + m_rulerTop, m_rulerColor));

			// key line
			for (int i = 0; i <= keyCount; i++)
			{
				float xPos = i * keyWidth;
				if(xPos+m_rulerLeft>=0)
					m_rulerItems.emplace_back(EditorApi.qGraphicsSceneAddLine(m_graphicsScene, xPos + m_rulerLeft, 18.f + m_rulerTop, xPos + m_rulerLeft, m_rulerHeight + m_rulerTop, m_rulerColor));
			}

			// draw Text
			for (int i = 0; i <= keyCount; i++)
			{
				if (i % 2 == 0)
				{
					i32 time;
					float value;
					Vector2 textPos(i * keyWidth + m_rulerLeft, 5.f + m_rulerTop);
					calcKeyTimeAndValueByPos(textPos, time, value);
					if (time >= 0)
					{
						QGraphicsItem* textItem = m_graphicsScene->addSimpleText(StringUtil::Format("%d", time).c_str());
						if (textItem)
						{
							//float halfWidth = EditorApi.qGraphicsItemWidth(textItem) * 0.4f /*0.5f*/;
							EditorApi.qGraphicsItemSetPos(textItem, textPos.x, textPos.y);
							m_rulerItems.push_back(textItem);
						}
					}
				}
			}

			// set z value
			for (QGraphicsItem* item : m_rulerItems)
			{
				EditorApi.qGraphicsItemSetZValue(item, 200.f);
			}

			// draw value
			drawRulerVertical();
		}
	}

	void TimelinePanel::drawRulerVertical()
	{
		for (QGraphicsItem* item : m_rulerHItems)
		{
			EditorApi.qGraphicsSceneDeleteItem(m_graphicsScene, item);
		}
		m_rulerHItems.clear();

		const float keyWidth = 20;	// pixels
		const int   keyCount = 50;

		// ruler bottom
		Color bgColor; bgColor.setRGBA(83, 83, 83, 255);
		//m_rulerItems.emplace_back(qGraphicsSceneAddRect(m_graphicsScene, std::max<float>(float(-keyWidth) + m_rulerLeft, -keyWidth), -1 + m_rulerTop, float(keyCount * keyWidth) + keyWidth, m_rulerHeight, bgColor));
		m_rulerItems.emplace_back(EditorApi.qGraphicsSceneAddLine(m_graphicsScene, std::max<float>(keyWidth + m_rulerLeft, 0), m_rulerHeight + m_rulerTop, std::max<float>(keyWidth + m_rulerLeft, 0), float(keyCount * keyWidth) + keyWidth + m_rulerLeft, m_rulerColor));

		//// key line
		//for (int i = 0; i <= keyCount; i++)
		//{
		//	float xPos = i * keyWidth;
		//	if (xPos + m_rulerLeft >= 0)
		//		m_rulerItems.emplace_back(qGraphicsSceneAddLine(m_graphicsScene, xPos + m_rulerLeft, 18.f + m_rulerTop, xPos + m_rulerLeft, m_rulerHeight + m_rulerTop, m_rulerColor));
		//}

		//// draw Text
		//for (int i = 0; i <= keyCount; i++)
		//{
		//	if (i % 2 == 0)
		//	{
		//		i32 time;
		//		float value;
		//		Vector2 textPos(i * keyWidth + m_rulerLeft, 5.f + m_rulerTop);
		//		calcKeyTimeAndValueByPos(textPos, time, value);
		//		if (time >= 0)
		//		{
		//			QGraphicsItem* textItem = qGraphicsSceneAddSimpleText(m_graphicsScene, StringUtil::Format("%d", time).c_str(), m_rulerColor);
		//			if (textItem)
		//			{
		//				float halfWidth = qGraphicsItemWidth(textItem) * 0.4f /*0.5f*/;
		//				qGraphicsItemSetPos(textItem, textPos.x, textPos.y);
		//				m_rulerItems.emplace_back(textItem);
		//			}
		//		}
		//	}
		//}

		//// set z value
		//for (QGraphicsItem* item : m_rulerItems)
		//{
		//	qGraphicsItemSetZValue(item, 200.f);
		//}
	}

	void TimelinePanel::onSwitchCurveVisibility()
	{
		m_curveVisibles[0] = EditorApi.qToolButtonIsChecked(EditorApi.qFindChild(m_ui, "m_curveXVisible"));
		m_curveVisibles[1] = EditorApi.qToolButtonIsChecked(EditorApi.qFindChild(m_ui, "m_curveYVisible"));
		m_curveVisibles[2] = EditorApi.qToolButtonIsChecked(EditorApi.qFindChild(m_ui, "m_curveZVisible"));
		m_curveVisibles[3] = EditorApi.qToolButtonIsChecked(EditorApi.qFindChild(m_ui, "m_curveWVisible"));

		for (size_t i = 0; i < m_curveItems.size(); i++)
		{
			if (m_curveItems[i])
			{
				EditorApi.qGraphicsItemSetVisible(m_curveItems[i], m_curveVisibles[i]);
			}

			for (QGraphicsItem* keyItem : m_curveKeyItems[i])
			{
				EditorApi.qGraphicsItemSetVisible( keyItem, m_curveVisibles[i]);
			}
		}
	}

	void TimelinePanel::onRightClickGraphicsView()
	{
		// clear menu
		if (m_keyEditMenu)
		{
			EditorApi.qDeleteWidget(m_keyEditMenu);
		}

		AnimProperty* animProperty = m_timeline->getProperty(m_currentEditAnim, m_currentEditObjectPath, m_currentEditPropertyChain);
		if (animProperty)
		{
			if (animProperty->getType() == AnimProperty::Type::Bool)
			{
				m_keyEditMenu = EditorApi.qMenuNew(m_ui);

				EditorApi.qMenuAddAction(m_keyEditMenu, EditorApi.qFindChildAction(m_ui, "m_actionAddBoolKeyToCurve"));
				EditorApi.qConnectAction(EditorApi.qFindChildAction(m_ui, "m_actionAddBoolKeyToCurve"), QSIGNAL(triggered()), this, createMethodBind(&TimelinePanel::onAddBoolKeyToCurve));
			}
			else if (animProperty->getType() == AnimProperty::Type::Vector3)
			{
				m_keyEditMenu = EditorApi.qMenuNew(m_ui);

				EditorApi.qMenuAddAction(m_keyEditMenu, EditorApi.qFindChildAction(m_ui, "m_actionAddKeyToCurveRed"));
				EditorApi.qMenuAddAction(m_keyEditMenu, EditorApi.qFindChildAction(m_ui, "m_actionAddKeyToCurveGreen"));
				EditorApi.qMenuAddAction(m_keyEditMenu, EditorApi.qFindChildAction(m_ui, "m_actionAddKeyToCurveBlue"));
				EditorApi.qConnectAction(EditorApi.qFindChildAction(m_ui, "m_actionAddKeyToCurveRed"), QSIGNAL(triggered()), this, createMethodBind(&TimelinePanel::onAddKeyToCurveRed));
				EditorApi.qConnectAction(EditorApi.qFindChildAction(m_ui, "m_actionAddKeyToCurveGreen"), QSIGNAL(triggered()), this, createMethodBind(&TimelinePanel::onAddKeyToCurveGreen));
				EditorApi.qConnectAction(EditorApi.qFindChildAction(m_ui, "m_actionAddKeyToCurveBlue"), QSIGNAL(triggered()), this, createMethodBind(&TimelinePanel::onAddKeyToCurveBlue));
			}
			else if (animProperty->getType() == AnimProperty::Type::String)
			{
				m_keyEditMenu = EditorApi.qMenuNew(m_ui);

				EditorApi.qMenuAddAction(m_keyEditMenu, EditorApi.qFindChildAction(m_ui, "m_actionAddStringKeyToCurve"));
				EditorApi.qConnectAction(EditorApi.qFindChildAction(m_ui, "m_actionAddStringKeyToCurve"), QSIGNAL(triggered()), this, createMethodBind(&TimelinePanel::onAddStringKeyToCurve));
			}
		}

		// record cursor pos
		m_keyEditCursorScenePos = EditorApi.qObjectGetEventAll(m_graphicsScene).graphicsSceneMouseEvent.scenePos;

		// show menu
		EditorApi.qMenuExec(m_keyEditMenu);
	}

	void TimelinePanel::onGraphicsSceneWheelEvent()
	{
		int delta = EditorApi.qObjectGetEventAll(m_graphicsScene).graphicsSceneWheelEvent.delta;
		
		m_millisecondPerPixel += delta > 0 ? -0.05f : 0.05f;
		m_millisecondPerPixel = Math::Clamp(m_millisecondPerPixel, 1.f, 50.f);

		m_unitsPerPixel += delta > 0 ? -0.05f : 0.05f;
		m_unitsPerPixel = Math::Clamp(m_unitsPerPixel, 1.f, 50.f);

		// refresh curve and key display
		refreshCurveDisplayToEditor(m_currentEditObjectPath, m_currentEditPropertyChain);
		refreshCurveKeyDisplayToEditor(m_currentEditObjectPath, m_currentEditPropertyChain);
	}

	bool TimelinePanel::calcKeyTimeAndValueByPos(const Vector2& pos, i32& time, float& value)
	{
		time = ui32(pos.x * m_millisecondPerPixel);
		value = (pos.y) * m_unitsPerPixel;

		return true;
	}

	bool TimelinePanel::calcKeyPosByTimeAndValue(i32 time, float value, Vector2& pos)
	{
		pos = Vector2(time / m_millisecondPerPixel, value / m_unitsPerPixel);

		return true;
	}

	void TimelinePanel::onAddBoolKeyToCurve()
	{
		i32 time;
		float value;
		if (calcKeyTimeAndValueByPos(m_keyEditCursorScenePos, time, value))
		{
			time = Math::Clamp(time, 0, 1000 * 60 * 60 * 24);
			m_timeline->addKey(m_currentEditAnim, m_currentEditObjectPath, StringUtil::ToString(m_currentEditPropertyChain), time, true);
		}

		// refresh curve and key display
		refreshCurveDisplayToEditor(m_currentEditObjectPath, m_currentEditPropertyChain);
		refreshCurveKeyDisplayToEditor(m_currentEditObjectPath, m_currentEditPropertyChain);
	}

	void TimelinePanel::onAddStringKeyToCurve()
	{
		i32 time;
		float value;
		if (calcKeyTimeAndValueByPos(m_keyEditCursorScenePos, time, value))
		{
			time = Math::Clamp(time, 0, 1000 * 60 * 60 * 24);

			Variant::Type variantType = m_timeline->getAnimPropertyVariableType(m_currentEditObjectPath, m_currentEditPropertyChain);
			if (variantType == Variant::Type::ResourcePath)
			{
				Variant currentValue;
				if (TimelinePanelUtil::getProertyValue(m_timeline, m_currentEditObjectPath, m_currentEditPropertyChain, currentValue))
				{
					String exts = currentValue.toResPath().getSupportExts().c_str();
					String path = Editor::instance()->selectAResObject(exts.c_str());
					m_timeline->addKey(m_currentEditAnim, m_currentEditObjectPath, StringUtil::ToString(m_currentEditPropertyChain), time, path);
				}
			}
		}

		// refresh curve and key display
		refreshCurveDisplayToEditor(m_currentEditObjectPath, m_currentEditPropertyChain);
		refreshCurveKeyDisplayToEditor(m_currentEditObjectPath, m_currentEditPropertyChain);
	}

	void TimelinePanel::onAddKeyToCurveRed()
	{
		addKeyToCurve(0);
	}

	void TimelinePanel::onAddKeyToCurveGreen()
	{
		addKeyToCurve(1);
	}

	void TimelinePanel::onAddKeyToCurveBlue()
	{
		addKeyToCurve(2);
	}

	void TimelinePanel::onAddKeyToCurveWhite()
	{
		addKeyToCurve(3);
	}

	void TimelinePanel::addKeyToCurve(int curveIdx)
	{
		i32 time;
		float value;
		if (calcKeyTimeAndValueByPos(m_keyEditCursorScenePos, time, value))
		{
			time = Math::Clamp(time, 0, 1000 * 60 * 60 * 24);
			m_timeline->addKey(m_currentEditAnim, m_currentEditObjectPath, StringUtil::ToString(m_currentEditPropertyChain), curveIdx, time, value);
		}

		// refresh curve and key display
		refreshCurveDisplayToEditor(m_currentEditObjectPath, m_currentEditPropertyChain);
		refreshCurveKeyDisplayToEditor(m_currentEditObjectPath, m_currentEditPropertyChain);
	}

	void TimelinePanel::switchCurveEditor()
	{
		if (!m_currentEditAnim.empty() && !m_currentEditObjectPath.empty() && !m_currentEditPropertyChain.empty())
		{
			EditorApi.qWidgetSetEnable(EditorApi.qFindChild(m_ui, "m_graphicsView"), true);
		}
		else
		{
			EditorApi.qWidgetSetEnable(EditorApi.qFindChild(m_ui, "m_graphicsView"), false);
		}
	}

	void TimelinePanel::zoomCurve()
	{
		// query item total rect
		Rect totalRect;
		for (QGraphicsItem* item : m_curveItems)
		{
			Rect itemRect;
			EditorApi.qGraphicsItemSceneRect( item, itemRect);
			totalRect.merge(itemRect);
		}

		Rect viewRect;
		EditorApi.qGraphicsViewSceneRect(EditorApi.qFindChild( m_ui, "m_graphicsView"), viewRect);

		m_unitsPerPixel = (totalRect.getHeight() + 20.f) / viewRect.getHeight();

		// refresh curve display
		refreshCurveDisplayToEditor(m_currentEditObjectPath, m_currentEditPropertyChain);

		// refresh curve key display
		refreshCurveKeyDisplayToEditor(m_currentEditObjectPath, m_currentEditPropertyChain);

		// draw ruler
		drawRuler();
	}
#endif
}
