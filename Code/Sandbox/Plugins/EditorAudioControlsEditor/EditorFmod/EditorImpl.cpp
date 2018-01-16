// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "EditorImpl.h"

#include "ImplConnections.h"
#include "ProjectLoader.h"

#include <CryAudioImplFmod/GlobalData.h>
#include <CrySystem/ISystem.h>
#include <CryCore/StlUtils.h>
#include <CrySerialization/IArchiveHost.h>

namespace ACE
{
namespace Fmod
{
const string g_userSettingsFile = "%USER%/audiocontrolseditor_fmod.user";

//////////////////////////////////////////////////////////////////////////
EImpltemType TagToType(string const& tag)
{
	EImpltemType type = EImpltemType::Invalid;

	if (tag == CryAudio::s_szEventTag)
	{
		type = EImpltemType::Event;
	}
	else if (tag == CryAudio::Impl::Fmod::s_szParameterTag)
	{
		type = EImpltemType::Parameter;
	}
	else if (tag == CryAudio::Impl::Fmod::s_szSnapshotTag)
	{
		type = EImpltemType::Snapshot;
	}
	else if (tag == CryAudio::Impl::Fmod::s_szFileTag)
	{
		type = EImpltemType::Bank;
	}
	else if (tag == CryAudio::Impl::Fmod::s_szBusTag)
	{
		type = EImpltemType::Return;
	}

// Backwards compatibility will be removed before March 2019.
#if defined (USE_BACKWARDS_COMPATIBILITY)
	else if (tag == "FmodEvent")
	{
		type = EImpltemType::Event;
	}
	else if (tag == "FmodEventParameter")
	{
		type = EImpltemType::Parameter;
	}
	else if (tag == "FmodSnapshot")
	{
		type = EImpltemType::Snapshot;
	}
	else if (tag == "FmodSnapshotParameter")
	{
		type = EImpltemType::Parameter;
	}
	else if (tag == "FmodFile")
	{
		type = EImpltemType::Bank;
	}
	else if (tag == "FmodBus")
	{
		type = EImpltemType::Return;
	}
#endif // USE_BACKWARDS_COMPATIBILITY

	else
	{
		type = EImpltemType::Invalid;
	}

	return type;
}

//////////////////////////////////////////////////////////////////////////
char const* TypeToTag(EImpltemType const type)
{
	char const* tag = nullptr;

	switch (type)
	{
	case EImpltemType::Event:
		tag = CryAudio::s_szEventTag;
		break;
	case EImpltemType::Parameter:
		tag = CryAudio::Impl::Fmod::s_szParameterTag;
		break;
	case EImpltemType::Snapshot:
		tag = CryAudio::Impl::Fmod::s_szSnapshotTag;
		break;
	case EImpltemType::Bank:
		tag = CryAudio::Impl::Fmod::s_szFileTag;
		break;
	case EImpltemType::Return:
		tag = CryAudio::Impl::Fmod::s_szBusTag;
		break;
	default:
		tag = nullptr;
		break;
	}

	return tag;
}

//////////////////////////////////////////////////////////////////////////
CImplItem* SearchForControl(CImplItem* const pImplItem, string const& name, ItemType const type)
{
	CImplItem* pItem = nullptr;

	if ((pImplItem->GetName() == name) && (pImplItem->GetType() == type))
	{
		pItem =  pImplItem;
	}
	else
	{
		int const count = pImplItem->ChildCount();

		for (int i = 0; i < count; ++i)
		{
			CImplItem* const pFoundImplControl = SearchForControl(pImplItem->GetChildAt(i), name, type);

			if (pFoundImplControl != nullptr)
			{
				pItem = pFoundImplControl;
				break;
			}
		}
	}

	return pItem;
}

//////////////////////////////////////////////////////////////////////////
CImplSettings::CImplSettings()
	: m_projectPath(PathUtil::GetGameFolder() + CRY_NATIVE_PATH_SEPSTR AUDIO_SYSTEM_DATA_ROOT CRY_NATIVE_PATH_SEPSTR "fmod_project")
	, m_assetsPath(
		PathUtil::GetGameFolder() +
		CRY_NATIVE_PATH_SEPSTR
		AUDIO_SYSTEM_DATA_ROOT
		CRY_NATIVE_PATH_SEPSTR +
		CryAudio::Impl::Fmod::s_szImplFolderName +
		CRY_NATIVE_PATH_SEPSTR +
		CryAudio::s_szAssetsFolderName)
{
}

//////////////////////////////////////////////////////////////////////////
void CImplSettings::SetProjectPath(char const* szPath)
{
	m_projectPath = szPath;
	Serialization::SaveJsonFile(g_userSettingsFile.c_str(), *this);
}

//////////////////////////////////////////////////////////////////////////
void CImplSettings::Serialize(Serialization::IArchive& ar)
{
	ar(m_projectPath, "projectPath", "Project Path");
}

//////////////////////////////////////////////////////////////////////////
CEditorImpl::CEditorImpl()
{
	Serialization::LoadJsonFile(m_implSettings, g_userSettingsFile.c_str());
	gEnv->pAudioSystem->GetImplInfo(m_implInfo);
	m_implName = m_implInfo.name.c_str();
	m_implFolderName = CryAudio::Impl::Fmod::s_szImplFolderName;
}

//////////////////////////////////////////////////////////////////////////
CEditorImpl::~CEditorImpl()
{
	Clear();
}

//////////////////////////////////////////////////////////////////////////
void CEditorImpl::Reload(bool const preserveConnectionStatus)
{
	Clear();

	CProjectLoader(GetSettings()->GetProjectPath(), GetSettings()->GetAssetsPath(), m_rootControl);

	CreateControlCache(&m_rootControl);

	if (preserveConnectionStatus)
	{
		for (auto const& connection : m_connectionsByID)
		{
			if (connection.second > 0)
			{
				CImplItem* const pImplControl = GetControl(connection.first);

				if (pImplControl != nullptr)
				{
					pImplControl->SetConnected(true);
				}
			}
		}
	}
	else
	{
		m_connectionsByID.clear();
	}
}

//////////////////////////////////////////////////////////////////////////
CImplItem* CEditorImpl::GetControl(CID const id) const
{
	CImplItem* pImplItem = nullptr;

	if (id >= 0)
	{
		pImplItem = stl::find_in_map(m_controlsCache, id, nullptr);
	}

	return pImplItem;
}

//////////////////////////////////////////////////////////////////////////
char const* CEditorImpl::GetTypeIcon(CImplItem const* const pImplItem) const
{
	char const* szIconPath = "icons:Dialogs/dialog-error.ico";
	auto const type = static_cast<EImpltemType>(pImplItem->GetType());

	switch (type)
	{
	case EImpltemType::Folder:
		szIconPath = "icons:audio/fmod/folder_closed.ico";
		break;
	case EImpltemType::Event:
		szIconPath = "icons:audio/fmod/event.ico";
		break;
	case EImpltemType::Parameter:
		szIconPath = "icons:audio/fmod/tag.ico";
		break;
	case EImpltemType::Snapshot:
		szIconPath = "icons:audio/fmod/snapshot.ico";
		break;
	case EImpltemType::Bank:
		szIconPath = "icons:audio/fmod/bank.ico";
		break;
	case EImpltemType::Return:
		szIconPath = "icons:audio/fmod/return.ico";
		break;
	case EImpltemType::Group:
		szIconPath = "icons:audio/fmod/group.ico";
		break;
	default:
		szIconPath = "icons:Dialogs/dialog-error.ico";
		break;
	}

	return szIconPath;
}

//////////////////////////////////////////////////////////////////////////
string const& CEditorImpl::GetName() const
{
	return m_implName;
}

//////////////////////////////////////////////////////////////////////////
string const& CEditorImpl::GetFolderName() const
{
	return m_implFolderName;
}

//////////////////////////////////////////////////////////////////////////
bool CEditorImpl::IsTypeCompatible(ESystemItemType const systemType, CImplItem const* const pImplItem) const
{
	bool isCompatible = false;
	auto const implType = static_cast<EImpltemType>(pImplItem->GetType());

	switch (systemType)
	{
	case ESystemItemType::Trigger:
		isCompatible = (implType == EImpltemType::Event) || (implType == EImpltemType::Snapshot);
		break;
	case ESystemItemType::Parameter:
		isCompatible = (implType == EImpltemType::Parameter);
		break;
	case ESystemItemType::Preload:
		isCompatible = (implType == EImpltemType::Bank);
		break;
	case ESystemItemType::State:
		isCompatible = (implType == EImpltemType::Parameter);
		break;
	case ESystemItemType::Environment:
		isCompatible = (implType == EImpltemType::Return);
		break;
	default:
		isCompatible = false;
		break;
	}

	return isCompatible;
}

//////////////////////////////////////////////////////////////////////////
ESystemItemType CEditorImpl::ImplTypeToSystemType(CImplItem const* const pImplItem) const
{
	ESystemItemType systemType = ESystemItemType::Invalid;
	auto const implType = static_cast<EImpltemType>(pImplItem->GetType());

	switch (implType)
	{
	case EImpltemType::Event:
		systemType = ESystemItemType::Trigger;
		break;
	case EImpltemType::Parameter:
		systemType = ESystemItemType::Parameter;
		break;
	case EImpltemType::Snapshot:
		systemType = ESystemItemType::Trigger;
		break;
	case EImpltemType::Bank:
		systemType = ESystemItemType::Preload;
		break;
	case EImpltemType::Return:
		systemType = ESystemItemType::Environment;
		break;
	default:
		systemType = ESystemItemType::Invalid;
		break;
	}

	return systemType;
}

//////////////////////////////////////////////////////////////////////////
ConnectionPtr CEditorImpl::CreateConnectionToControl(ESystemItemType const controlType, CImplItem* const pImplItem)
{
	ConnectionPtr pConnection = nullptr;

	if (pImplItem != nullptr)
	{
		auto const type = static_cast<EImpltemType>(pImplItem->GetType());

		if ((type == EImpltemType::Event) || (type == EImpltemType::Snapshot))
		{
			pConnection = std::make_shared<CEventConnection>(pImplItem->GetId());
		}
		else if (type == EImpltemType::Parameter)
		{
			if (controlType == ESystemItemType::Parameter)
			{
				pConnection = std::make_shared<CParamConnection>(pImplItem->GetId());
			}
			else if (controlType == ESystemItemType::State)
			{
				pConnection = std::make_shared<CParamToStateConnection>(pImplItem->GetId());
			}
			else
			{
				pConnection = std::make_shared<CImplConnection>(pImplItem->GetId());
			}
		}
		else
		{
			pConnection = std::make_shared<CImplConnection>(pImplItem->GetId());
		}
	}

	return pConnection;
}

//////////////////////////////////////////////////////////////////////////
ConnectionPtr CEditorImpl::CreateConnectionFromXMLNode(XmlNodeRef pNode, ESystemItemType const controlType)
{
	ConnectionPtr pConnectionPtr = nullptr;

	if (pNode != nullptr)
	{
		auto const type = TagToType(pNode->getTag());

		if (type != EImpltemType::Invalid)
		{
			string name = pNode->getAttr(CryAudio::s_szNameAttribute);
			string path = pNode->getAttr(CryAudio::Impl::Fmod::s_szPathAttribute);
			string localizedAttribute = pNode->getAttr(CryAudio::Impl::Fmod::s_szLocalizedAttribute);
#if defined (USE_BACKWARDS_COMPATIBILITY)
			if (name.IsEmpty() && pNode->haveAttr("fmod_name"))
			{
				name = pNode->getAttr("fmod_name");
			}

			if (path.IsEmpty() && pNode->haveAttr("fmod_path"))
			{
				path = pNode->getAttr("fmod_path");
			}

			if (localizedAttribute.IsEmpty() && pNode->haveAttr("fmod_localized"))
			{
				localizedAttribute = pNode->getAttr("fmod_localized");
			}
#endif // USE_BACKWARDS_COMPATIBILITY
			bool const isLocalized = (localizedAttribute.compareNoCase(CryAudio::Impl::Fmod::s_szTrueValue) == 0);

			if (!path.empty())
			{
				name = path + "/" + name;
			}

			CImplItem* pImplItem = GetItemFromPath(name);

			if ((pImplItem == nullptr) || (type != static_cast<EImpltemType>(pImplItem->GetType())))
			{
				// If control not found, create a placeholder.
				// We want to keep that connection even if it's not in the middleware as the user could
				// be using the engine without the fmod project

				path = "";
				int const pos = name.find_last_of("/");

				if (pos != string::npos)
				{
					path = name.substr(0, pos);
					name = name.substr(pos + 1, name.length() - pos);
				}

				pImplItem = CreateItem(type, CreatePlaceholderFolderPath(path), name);

				if (pImplItem != nullptr)
				{
					pImplItem->SetPlaceholder(true);
				}
			}

			switch (type)
			{
			case EImpltemType::Event:
			case EImpltemType::Snapshot:
				{
					string eventType = pNode->getAttr(CryAudio::s_szTypeAttribute);

#if defined (USE_BACKWARDS_COMPATIBILITY)
					if (eventType.IsEmpty() && pNode->haveAttr("fmod_event_type"))
					{
						eventType = pNode->getAttr("fmod_event_type");
					}
#endif // USE_BACKWARDS_COMPATIBILITY
					auto const pConnection = std::make_shared<CEventConnection>(pImplItem->GetId());
					pConnection->m_type = (eventType == CryAudio::Impl::Fmod::s_szStopValue) ? EEventType::Stop : EEventType::Start;
					pConnectionPtr = pConnection;
				}
				break;
			case EImpltemType::Parameter:
				{
					if (controlType == ESystemItemType::Parameter)
					{
						std::shared_ptr<CParamConnection> const pConnection = std::make_shared<CParamConnection>(pImplItem->GetId());
						float mult = 1.0f;
						float shift = 0.0f;

						if (pNode->haveAttr(CryAudio::Impl::Fmod::s_szMutiplierAttribute))
						{
							string const value = pNode->getAttr(CryAudio::Impl::Fmod::s_szMutiplierAttribute);
							mult = (float)std::atof(value.c_str());
						}
#if defined (USE_BACKWARDS_COMPATIBILITY)
						else if (pNode->haveAttr("fmod_value_multiplier"))
						{
							string const value = pNode->getAttr("fmod_value_multiplier");
							mult = (float)std::atof(value.c_str());
						}
#endif // USE_BACKWARDS_COMPATIBILITY
						if (pNode->haveAttr(CryAudio::Impl::Fmod::s_szShiftAttribute))
						{
							string const value = pNode->getAttr(CryAudio::Impl::Fmod::s_szShiftAttribute);
							shift = (float)std::atof(value.c_str());
						}
#if defined (USE_BACKWARDS_COMPATIBILITY)
						else if (pNode->haveAttr("fmod_value_shift"))
						{
							string const value = pNode->getAttr("fmod_value_shift");
							shift = (float)std::atof(value.c_str());
						}
#endif // USE_BACKWARDS_COMPATIBILITY
						pConnection->m_mult = mult;
						pConnection->m_shift = shift;
						pConnectionPtr = pConnection;
					}
					else if (controlType == ESystemItemType::State)
					{
						std::shared_ptr<CParamToStateConnection> const pConnection = std::make_shared<CParamToStateConnection>(pImplItem->GetId());
						string value = pNode->getAttr(CryAudio::Impl::Fmod::s_szValueAttribute);
#if defined (USE_BACKWARDS_COMPATIBILITY)
						if (value.IsEmpty() && pNode->haveAttr("fmod_value"))
						{
							value = pNode->getAttr("fmod_value");
						}
#endif
						pConnection->m_value = (float)std::atof(value.c_str());
						pConnectionPtr = pConnection;
					}
				}
				break;
			case EImpltemType::Bank:
			case EImpltemType::Return:
				{
					pConnectionPtr = std::make_shared<CImplConnection>(pImplItem->GetId());
				}
				break;
			}
		}
	}

	return pConnectionPtr;
}

//////////////////////////////////////////////////////////////////////////
XmlNodeRef CEditorImpl::CreateXMLNodeFromConnection(ConnectionPtr const pConnection, ESystemItemType const controlType)
{
	XmlNodeRef pConnectionNode = nullptr;

	CImplItem* const pImplControl = GetControl(pConnection->GetID());

	if (pImplControl != nullptr)
	{
		auto const type = static_cast<EImpltemType>(pImplControl->GetType());
		pConnectionNode = GetISystem()->CreateXmlNode(TypeToTag(type));

		switch (type)
		{
		case EImpltemType::Event:
		case EImpltemType::Snapshot:
			{
				pConnectionNode->setAttr(CryAudio::s_szNameAttribute, GetPathName(pImplControl));
				auto const pEventConnection = static_cast<const CEventConnection*>(pConnection.get());

				if ((pEventConnection != nullptr) && (pEventConnection->m_type == EEventType::Stop))
				{
					pConnectionNode->setAttr(CryAudio::s_szTypeAttribute, CryAudio::Impl::Fmod::s_szStopValue);
				}
			}
			break;
		case EImpltemType::Return:
			{
				pConnectionNode->setAttr(CryAudio::s_szNameAttribute, GetPathName(pImplControl));
			}
			break;
		case EImpltemType::Parameter:
			{
				pConnectionNode->setAttr(CryAudio::s_szNameAttribute, pImplControl->GetName());
				pConnectionNode->setAttr(CryAudio::Impl::Fmod::s_szPathAttribute, GetPathName(pImplControl->GetParent()));

				if (controlType == ESystemItemType::State)
				{
					auto const pStateConnection = static_cast<const CParamToStateConnection*>(pConnection.get());

					if (pStateConnection != nullptr)
					{
						pConnectionNode->setAttr(CryAudio::Impl::Fmod::s_szValueAttribute, pStateConnection->m_value);
					}
				}
				else if (controlType == ESystemItemType::Parameter)
				{
					auto const pParamConnection = static_cast<const CParamConnection*>(pConnection.get());

					if (pParamConnection->m_mult != 1.0f)
					{
						pConnectionNode->setAttr(CryAudio::Impl::Fmod::s_szMutiplierAttribute, pParamConnection->m_mult);
					}

					if (pParamConnection->m_shift != 0.0f)
					{
						pConnectionNode->setAttr(CryAudio::Impl::Fmod::s_szShiftAttribute, pParamConnection->m_shift);
					}
				}
			}
			break;
		case EImpltemType::Bank:
			{
				pConnectionNode->setAttr(CryAudio::s_szNameAttribute, GetPathName(pImplControl));
			}
			break;
		}
	}

	return pConnectionNode;
}

//////////////////////////////////////////////////////////////////////////
void CEditorImpl::EnableConnection(ConnectionPtr const pConnection)
{
	CImplItem* const pImplControl = GetControl(pConnection->GetID());

	if (pImplControl != nullptr)
	{
		++m_connectionsByID[pImplControl->GetId()];
		pImplControl->SetConnected(true);
	}
}

//////////////////////////////////////////////////////////////////////////
void CEditorImpl::DisableConnection(ConnectionPtr const pConnection)
{
	CImplItem* const pImplControl = GetControl(pConnection->GetID());

	if (pImplControl != nullptr)
	{
		int connectionCount = m_connectionsByID[pImplControl->GetId()] - 1;

		if (connectionCount <= 0)
		{
			connectionCount = 0;
			pImplControl->SetConnected(false);
		}

		m_connectionsByID[pImplControl->GetId()] = connectionCount;
	}
}

//////////////////////////////////////////////////////////////////////////
void CEditorImpl::Clear()
{
	// Delete all the controls
	for (auto const& controlPair : m_controlsCache)
	{
		CImplItem const* const pImplControl = controlPair.second;

		if (pImplControl != nullptr)
		{
			delete pImplControl;
		}
	}

	m_controlsCache.clear();

	// Clean up the root control
	m_rootControl = CImplItem();
}

//////////////////////////////////////////////////////////////////////////
void CEditorImpl::CreateControlCache(CImplItem const* const pParent)
{
	if (pParent != nullptr)
	{
		size_t const count = pParent->ChildCount();

		for (size_t i = 0; i < count; ++i)
		{
			CImplItem* const pChild = pParent->GetChildAt(i);

			if (pChild != nullptr)
			{
				m_controlsCache[pChild->GetId()] = pChild;
				CreateControlCache(pChild);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
CImplItem* CEditorImpl::CreateItem(EImpltemType const type, CImplItem* const pParent, string const& name)
{
	CID const id = GetId(type, name, pParent);

	CImplItem* pImplItem = GetControl(id);

	if (pImplItem != nullptr)
	{
		if (pImplItem->IsPlaceholder())
		{
			pImplItem->SetPlaceholder(false);
			CImplItem* pParentItem = pParent;

			while (pParentItem != nullptr)
			{
				pParentItem->SetPlaceholder(false);
				pParentItem = pParentItem->GetParent();
			}
		}
	}
	else
	{
		if (type == EImpltemType::Folder)
		{
			pImplItem = new CImplFolder(name, id);
		}
		else if (type == EImpltemType::Group)
		{
			pImplItem = new CImplGroup(name, id);
		}
		else
		{
			pImplItem = new CImplItem(name, id, static_cast<ItemType>(type));
		}
		if (pParent != nullptr)
		{
			pParent->AddChild(pImplItem);
			pImplItem->SetParent(pParent);
		}
		else
		{
			m_rootControl.AddChild(pImplItem);
			pImplItem->SetParent(&m_rootControl);
		}

		m_controlsCache[id] = pImplItem;

	}

	return pImplItem;
}

//////////////////////////////////////////////////////////////////////////
CID CEditorImpl::GetId(EImpltemType const type, string const& name, CImplItem* const pParent) const
{
	string const fullname = GetTypeName(type) + GetPathName(pParent) + CRY_NATIVE_PATH_SEPSTR + name;
	return CryAudio::StringToId(fullname);
}

//////////////////////////////////////////////////////////////////////////
string CEditorImpl::GetPathName(CImplItem const* const pImplItem) const
{
	string pathName = "";

	if (pImplItem != nullptr)
	{
		string fullname = pImplItem->GetName();
		CImplItem const* pParent = pImplItem->GetParent();

		while ((pParent != nullptr) && (pParent != &m_rootControl))
		{
			// The id needs to represent the full path, as we can have items with the same name in different folders
			fullname = pParent->GetName() + "/" + fullname;
			pParent = pParent->GetParent();
		}

		pathName = fullname;
	}

	return pathName;
}

//////////////////////////////////////////////////////////////////////////
string CEditorImpl::GetTypeName(EImpltemType const type) const
{
	string name = "";

	switch (type)
	{
	case EImpltemType::Folder:
		name = "folder:";
		break;
	case EImpltemType::Event:
		name = "event:";
		break;
	case EImpltemType::Parameter:
		name = "parameter:";
		break;
	case EImpltemType::Snapshot:
		name = "snapshot:";
		break;
	case EImpltemType::Bank:
		name = "bank:";
		break;
	case EImpltemType::Return:
		name = "return:";
		break;
	case EImpltemType::Group:
		name = "group:";
		break;
	default:
		name = "";
		break;
	}

	return name;
}

//////////////////////////////////////////////////////////////////////////
CImplItem* CEditorImpl::GetItemFromPath(string const& fullpath)
{
	CImplItem* pImplItem = &m_rootControl;
	int start = 0;
	string token = fullpath.Tokenize("/", start);

	while (!token.empty())
	{
		token.Trim();
		CImplItem* pChild = nullptr;
		int const size = pImplItem->ChildCount();

		for (int i = 0; i < size; ++i)
		{
			CImplItem* const pNextChild = pImplItem->GetChildAt(i);

			if ((pNextChild != nullptr) && (pNextChild->GetName() == token))
			{
				pChild = pNextChild;
				break;
			}
		}

		if (pChild != nullptr)
		{
			pImplItem = pChild;
			token = fullpath.Tokenize("/", start);
		}
		else
		{
			pImplItem = nullptr;
			break;
		}
	}

	return pImplItem;
}

//////////////////////////////////////////////////////////////////////////
CImplItem* CEditorImpl::CreatePlaceholderFolderPath(string const& path)
{
	CImplItem* pImplItem = &m_rootControl;
	int start = 0;
	string token = path.Tokenize("/", start);

	while (!token.empty())
	{
		token.Trim();
		CImplItem* pFoundChild = nullptr;
		int const size = pImplItem->ChildCount();

		for (int i = 0; i < size; ++i)
		{
			CImplItem* const pChild = pImplItem->GetChildAt(i);

			if ((pChild != nullptr) && (pChild->GetName() == token))
			{
				pFoundChild = pChild;
				break;
			}
		}

		if (pFoundChild == nullptr)
		{
			pFoundChild = CreateItem(EImpltemType::Folder, pImplItem, token);
			pFoundChild->SetPlaceholder(true);
		}

		pImplItem = pFoundChild;
		token = path.Tokenize("/", start);
	}

	return pImplItem;
}
} // namespace Fmod
} // namespace ACE