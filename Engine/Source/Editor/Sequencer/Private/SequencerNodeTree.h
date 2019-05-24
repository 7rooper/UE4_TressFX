// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "UObject/ObjectKey.h"
#include "Tree/CurveEditorTree.h"
#include "DisplayNodes/SequencerDisplayNode.h"
#include "SectionHandle.h"

class FSequencer;
class FCurveEditor;
class FSequencerDisplayNode;
class FSequencerObjectBindingNode;
class FSequencerSectionKeyAreaNode;
class FSequencerTrackNode;
class FSequencerFolderNode;
class ISequencerTrackEditor;
class UMovieSceneFolder;
class UMovieSceneTrack;
class UMovieScene;
struct FMovieSceneBinding;
struct FCurveEditorTreeItemID;

/**
 * Represents a tree of sequencer display nodes, used to populate the Sequencer UI with MovieScene data
 */
class FSequencerNodeTree : public TSharedFromThis<FSequencerNodeTree>
{
public:
	DECLARE_MULTICAST_DELEGATE(FOnUpdated);

public:

	FSequencerNodeTree(FSequencer& InSequencer);

	/**
	 * Updates the tree with sections from a MovieScene
	 */
	void Update();

	/**
	 * Access this tree's symbolic root node
	 */
	TSharedRef<FSequencerDisplayNode> GetRootNode() const;

	/**
	 * @return The root nodes of the tree
	 */
	const TArray< TSharedRef<FSequencerDisplayNode> >& GetRootNodes() const;

	/** @return Whether or not there is an active filter */
	bool HasActiveFilter() const { return !FilterString.IsEmpty(); }

	/**
	 * Returns whether or not a node is filtered
	 *
	 * @param Node	The node to check if it is filtered
	 */
	bool IsNodeFiltered( const TSharedRef<const FSequencerDisplayNode> Node ) const;
	
	/**
	 * Filters the nodes based on the passed in filter terms
	 *
	 * @param InFilter	The filter terms
	 */
	void FilterNodes( const FString& InFilter );

	/**
	 * @return All nodes in a flat array
	 */
	TArray< TSharedRef<FSequencerDisplayNode> > GetAllNodes() const;

	/** Gets the parent sequencer of this tree */
	FSequencer& GetSequencer() {return Sequencer;}

	/**
	 * Saves the expansion state of a display node
	 *
	 * @param Node		The node whose expansion state should be saved
	 * @param bExpanded	The new expansion state of the node
	 */
	void SaveExpansionState( const FSequencerDisplayNode& Node, bool bExpanded );

	/**
	 * Gets the saved expansion state of a display node
	 *
	 * @param Node	The node whose expansion state may have been saved
	 * @return true if the node should be expanded, false otherwise	
	 */
	bool GetSavedExpansionState( const FSequencerDisplayNode& Node ) const;

	/**
	 * Get the default expansion state for the specified node, where its state has not yet been saved
	 *
	 * @return true if the node is to be expanded, false otherwise
	 */
	bool GetDefaultExpansionState( const FSequencerDisplayNode& Node ) const;

	/**
	 * Set the single hovered node in the tree
	 */
	void SetHoveredNode(const TSharedPtr<FSequencerDisplayNode>& InHoveredNode);

	/**
	 * Get the single hovered node in the tree, possibly nullptr
	 */
	const TSharedPtr<FSequencerDisplayNode>& GetHoveredNode() const;

	/*
	 * Find the object binding node with the specified GUID
	 */
	TSharedPtr<FSequencerObjectBindingNode> FindObjectBindingNode(const FGuid& BindingID) const;

	/*
	 * Gets a multicast delegate which is called whenever the node tree has been updated.
	 */
	FOnUpdated& OnUpdated() { return OnUpdatedDelegate; }

	/** Make the contents of the given node have the root as their parent again instead of their current parent. */
	void MoveDisplayNodeToRoot(TSharedRef<FSequencerDisplayNode>& Node);

	/** Sorts all nodes and their descendants by category then alphabetically.*/
	void SortAllNodesAndDescendants();

public:

	/**
	 * Finds the section handle relating to the specified section object, or Null if one was not found (perhaps, if it's filtered away)
	 */
	TOptional<FSectionHandle> GetSectionHandle(const UMovieSceneSection* Section) const;

private:

	/** Population algorithm utilities */
	void RefreshNodes(UMovieScene* MovieScene);

	enum class ETrackType { Master, Object };

	TSharedPtr<FSequencerTrackNode> CreateOrUpdateTrack(UMovieSceneTrack* Track, ETrackType TrackType);
	TSharedRef<FSequencerFolderNode> CreateOrUpdateFolder(UMovieSceneFolder* Folder, const TSortedMap<FGuid, const FMovieSceneBinding*>& AllBindings, const TSortedMap<FGuid, FGuid>& ChildToParentBinding, const UMovieScene* InMovieScene);
	TSharedPtr<FSequencerObjectBindingNode> CreateOrUpdateObjectBinding(const FGuid& BindingID, const TSortedMap<FGuid, const FMovieSceneBinding*>& AllBindings, const TSortedMap<FGuid, FGuid>& ChildToParentBinding, const UMovieScene* InMovieScene);

	/**
	 * Creates section handles for all the sections contained in the specified track
	 */
	void UpdateSectionHandles(TSharedRef<FSequencerTrackNode> TrackNode);

private:
	/**
	 * Finds or adds a type editor for the track
	 *
	 * @param Track	The type to find an editor for
	 * @return The editor for the type
	 */
	TSharedRef<ISequencerTrackEditor> FindOrAddTypeEditor( UMovieSceneTrack* Track );

	/**
	 * Update the curve editor tree to include anything of relevance from this tree
	 */
	void UpdateCurveEditorTree();

	/**
	 * Add the specified node to the curve editor, including all its parents if necessary
	 */
	FCurveEditorTreeItemID AddToCurveEditor(TSharedRef<FSequencerDisplayNode> Node, FCurveEditor* CurveEditor);

	/**
	 * Checks whether the specified key area node contains key areas that can create curve models
	 */
	bool KeyAreaHasCurves(const FSequencerSectionKeyAreaNode& KeyAreaNode) const;

private:

	/** Symbolic root node that contains the actual displayed root nodes as children */
	TSharedRef<FSequencerDisplayNode> RootNode;

	/** A serially incrementing integer that is increased each time the tree is refreshed to track node relevance */
	uint32 SerialNumber;
	/** Map from FMovieSceneBinding::GetObjectGuid to display node */
	TMap<FGuid,      TSharedPtr<FSequencerObjectBindingNode>> ObjectBindingToNode;
	/** Map from UMovieSceneTrack object key to display node for any track (object tracks or master tracks) */
	TMap<FObjectKey, TSharedPtr<FSequencerTrackNode>>         TrackToNode;
	/** Map from UMovieSceneFolder object key to display node for any folder (root or child folder) */
	TMap<FObjectKey, TSharedPtr<FSequencerFolderNode>>        FolderToNode;

	/** Map from UMovieSceneSection* to its UI handle */
	TMap<FObjectKey, FSectionHandle> SectionToHandle;

	/** Tools for building movie scene section layouts.  One tool for each track */
	TMap< UMovieSceneTrack*, TSharedPtr<ISequencerTrackEditor> > EditorMap;
	/** Set of all filtered nodes */
	TSet< TSharedRef<const FSequencerDisplayNode> > FilteredNodes;
	/** Cardinal hovered node */
	TSharedPtr<FSequencerDisplayNode> HoveredNode;
	/** Active filter string if any */
	FString FilterString;
	/** Sequencer interface */
	FSequencer& Sequencer;
	/** Display node -> curve editor tree item ID mapping */
	TMap<TWeakPtr<FSequencerDisplayNode>, FCurveEditorTreeItemID> CurveEditorTreeItemIDs;
	/** A multicast delegate which is called whenever the node tree has been updated. */
	FOnUpdated OnUpdatedDelegate;
};
