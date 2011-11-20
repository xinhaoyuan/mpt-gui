
var currentPlayTreeName = null;
var currentPlayTreeView;

function ItemName(item)
{
	var result = "";

	if (item.Title)
		result = item.Title;

	if (item.Artist)
		result = item.Artist + " - " + result;

	return result;
}

function CreatePlaytree()
{
	MergeSelection();
}

function MergeSelection()
{
	var darr = [];
	var items = $("#tree-selection-list .selection-item");
	if (items.size() == 0) return [];

	items.each(function(index, object){
		darr.push({ "id" : object.id, "title" : $(object).text() });
	});

	darr.sort(function(a, b) { return a.id - b.id; });

	var content = $("<div></div>").hide();
	content.addClass("tree-selection-content");
	var arr = [];

	for (var i = 0; i < darr.length; ++ i)
	{
		if (i == 0 || darr[i].id != darr[i - 1].id)
		{
			content.append("<div class=\"selection-item\" id=\"" + darr[i].id + "\">" + darr[i].title + "</div>");
			arr.push(darr[i].id);
		}
	}
	
	var title = $("<li><a href=# id=\"expand\">Merged selection</a>. <a href=# id=\"remove\">REMOVE</a></li>");
	title.children("#expand").click(function() {
		content.toggle();
	});
	title.children("#remove").click(function() {
		title.remove();
	});

	title.append(content);
	
	$("#tree-selection-list").children().remove();
	$("#tree-selection-list").append(title);

	return arr;
}

function ClearSelection()
{
	$("#tree-selection-list").contents().remove();	
}

function PlaySelection()
{
	var arr = MergeSelection();

	if (arr.length == 0) return;

	sys.SendToPlayer(arr);
}

function AddItemsRecur(o, v)
{
	$(v.subviews).each(function(index, view) { AddItemsRecur(o, view); } );
	$(v.items).each(function(index, item) { o.append("<div class=\"selection-item\" id=\"" + item.id + "\">" + ItemName(item) + "</div>"); } );
}

function ExportSelection()
{
	var content = $("<div></div>").hide();
	var items = $("#tree-view .ui-selected");
	if (items.size() == 0) return;

	items.each(function(index, object) {
		var view = $(object).data("view");
		if (view != undefined)
		{
			AddItemsRecur(content, view);
		}
		else
		{
			content.append("<div class=\"selection-item\" id=\"" + object.id + "\">" + $(object).text() + "</div>");
		}
	});

	if (content.contents().size() == 0) return;

	content.addClass("tree-selection-content");

	var title = $("<li><a href=# id=\"expand\">Selection from " + currentPlayTreeName + "</a>. <a href=# id=\"remove\">REMOVE</a></li>");
	title.children("#expand").click(function() {
		content.toggle();
	});
	title.children("#remove").click(function() {
		title.remove();
	});

	title.append(content);

	$("#tree-selection-list").append(title);
}

function InsertSelection()
{
	var arr = MergeSelection();
	if (arr.length == 0) return;

	sys.PlayTreeInsert(currentPlayTreeName, arr);
	OpenPlayTree(null);
}

function RemoveSelection()
{
	var arr = MergeSelection();
	if (arr.length == 0) return;

	// sys.PlayTreeRemove(currentPlayTreeName, arr);
	OpenPlayTree(null);
}

function UpdateSelection()
{
	var arr = MergeSelection();
	if (arr.length == 0) return;

	// sys.PlayTreeUpdate(currentPlayTreeName, arr);
	OpenPlayTree(null);
}

function ConstructViewShow(view, ele)
{
	ele.contents().remove();
	var dirsel = $("<div></div>");
	$.each(view.subviews, function(index, subview) {
		var nameele = $("<a href=#><img src=\"images/subview.png\"/>" + subview.name + "</a>");
		var saele = $("<span><img src=\"images/sa.png\"/></span>");
		var titleele = $("<div></div>").append(saele).append(nameele);

		titleele.data("view", subview);

		var subele = $("<div style=\"margin-left:10px;\"></div>");
		var expanded = false;
		var selected = false;

		nameele.click(function() {
			if (selected)
			{
				titleele.removeClass("ui-selected");
				selected = false;
			}

			if (expanded)
			{
				subele.contents().remove();
				expanded = false;
			}
			else
			{
				ConstructViewShow(subview, subele);
				expanded = true;
			}
		});

		saele.click(function() {
			if (expanded)
			{
				subele.contents().remove();
				expanded = false;
			}

			if (selected)
			{
				titleele.removeClass("ui-selected");
				selected = false;
			}
			else
			{
				titleele.addClass("ui-selected");
				selected = true;
			}
		});
		dirsel.append(titleele).append(subele);
	});

	var sel = $("<div></div>");
	$.each(view.items, function(index, item) {
		var nameele = $("<div id=\"" + item.id + "\"><img src=\"images/item.png\"/>" + ItemName(item) + "</div>");
		sel.append(nameele);
	});

	ele.append(dirsel);
	ele.append(sel);
	sel.selectable();
}

function OpenPlayTree(name)
{
	if (name == null && currentPlayTreeName != null)
	{
		sys.ClosePlayTree();
		currentPlayTreeView = sys.OpenPlayTree(currentPlayTreeName);
	}
	else
	{
		if (currentPlayTreeName != null && currentPlayTreeName != name)
		{
			sys.ClosePlayTree();
		}
		if (currentPlayTreeName != name)
			currentPlayTreeView = sys.OpenPlayTree(name);
	
		currentPlayTreeName = name;
	}

	if (currentPlayTreeName == null) return;
	ConstructViewShow(currentPlayTreeView, $("#tree-view"));
	$("#tree-tabs").tabs("select", 1);
}

function GetPlayTrees()
{
	var result = sys.GetPlayTreeList();
	var list = $("<div id=\"playtreelist\"></div>");
	$.each(result.list, function(index, value) {
		var item;
		if (value == "[LIBRARY]")
		{
			item = $("<div><a href=# id=\"open\">" + value + "</a></div>");
		}
		else
		{
			item = $("<div><a href=# id=\"open\">" + value + "</a><span style=\"float:right;\">" + 
					 "<a href=# id=\"reset\">RESET</a>|" + 
					 "<a href=# id=\"rename\">RENAME</a>|" + 
					 "<a href=# id=\"remove\">REMOVE</a></span></div>");
		}

		item.children("#open").click(function() {
			OpenPlayTree(value);
		});
		list.append(item);
	});
	$("#tree-sel-container").html(list.get(0));
}

$(document).ready(function(){
	$("#tree-tabs").tabs();
	$("#tree-sel").button();
	$("#tree-sel-refresh").button().click(function() {
		GetPlayTrees(); 
	});
	$("#tree-sel-create").button();
	$("#tree-view-create").button().click(CreatePlaytree);
	$("#tree-view-export").button().click(ExportSelection);
	$("#tree-view-insert").button().click(InsertSelection);
	$("#tree-view-remove").button().click(RemoveSelection);
	$("#tree-view-update").button().click(UpdateSelection);
	$("#selection-play").button().click(PlaySelection);
	$("#selection-clear").button().click(ClearSelection);
	$("#selection-merge").button().click(MergeSelection);

	GetPlayTrees();
});
