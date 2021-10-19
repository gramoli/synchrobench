package org.deuce.transform.util;

import java.util.HashMap;

public class IgnoreTree {
	
	final private TreeNode root = new TreeNode();
	
	public IgnoreTree( String names)
	{
		if( names == null)
			return;
		String[] splitNames = names.split(",");
		for( String name : splitNames){
			String[] parts = name.trim().split("\\.");
			root.add(parts, 0);
		}
	}

	public boolean contains( String name){
//		name
		String[] names = name.split("\\/");
		return root.contains(names, 0);
	}
	
	private static class TreeNode{
		
		final private HashMap<String, TreeNode> descendants = new HashMap<String, TreeNode>();
		private boolean isEnd = false;
		
		public boolean contains( String[] names, int index){

			TreeNode treeNode =  names.length <= index ? null : descendants.get( names[index]);
			if( treeNode == null){
				return isEnd ? true : false;
			}
				
			return treeNode.contains(names, index + 1);
		}
		
		public void add( String[] names, int index){
			
			if( names.length <= index || names[index].equals("*")){
				isEnd = true;
				return;
			}
			
			TreeNode treeNode = descendants.get( names[index]);
			if( treeNode == null)
			{
				treeNode = new TreeNode();
				descendants.put( names[index], treeNode);
			}
			
			treeNode.add(names, index + 1);
		}
	}
}
