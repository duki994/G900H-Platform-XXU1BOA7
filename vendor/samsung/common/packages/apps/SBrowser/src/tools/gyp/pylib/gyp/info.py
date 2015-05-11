DEBUG = False
DEBUG_PRINT_DEP_LIST = DEBUG & True
DEBUG_FIND_DEP = DEBUG & True

class Dep(object):
	def __init__(self, ref):
		self.target = ref
		self.dep = []
		
	def __repr__(self):
		return '<Dep: %r>' % self.target
		
	def getTarget(self):
		return self.target
		
	def getDep(self):
		return self.dep
		
	def setDep(self, dep_list):
		self.dep = dep_list
#end class Dep

# Find target_name is in dep_list(value)
# And return the target of dep
# @return: [List] list of targets have (target_name) dependents 
def findDepList(target_name, dep_list):
	if DEBUG: print 'findDepList.target_name: ', target_name
	results = []
	# search targest have (target_name) dependents
	for key, value in dep_list.items():
		if DEBUG_FIND_DEP:
			print 'findDepList.key:', key
			print 'findDepList.value:', value
		if target_name in value:
			if DEBUG: print 'findDepList: found ',  key
			dep = Dep(key)
			results.append(dep)
	
	return results
#end def findTarget


def findNodeInTree(target_name, dep):
	if DEBUG: print 'findNodeInTree.target_name:', target_name
	
	node = dep
	if target_name == node.getTarget():
		if DEBUG: print 'findNodeInTree: found target'
		return node
	else:
		node_list = node.getDep()
		for test_node in node_list:
			result = findNodeInTree(target_name, test_node)
			if result != None:
				return result
				
	return None
#end def findNodeInTree

def printTree(node, saveToFile=False, fout=None, depth=0):
	indent = '%d ' % depth
	if depth < 10: indent = '0' + indent
	for i in range(0, depth, 1):
		indent += "  "
	if saveToFile:
		fout.write(indent + node.getTarget() + '\n')
	else:
		print indent + node.getTarget()
	#end if saveToFile:
	
	for dep in node.getDep():
		printTree(dep, saveToFile, fout, depth+1)

#========================================================================================
#===========================    Main Function        ====================================
#========================================================================================

def printDependency(dep_list):
	# dep_list: [Dictionary] target(string) + dependents(List)
	# root_target: [Dep] root of build dependency tree
	# cur_target: [Dep] current target to set dependency
	# search_modules: [List] target(string)
	## dep_list = {}
	target_name = ':libsbrowser#' # need :, #
	root_target = None
	saveToFile = True
	
	if len(dep_list) == 0:
		# >>> load depedency info >>>
		#f = open('list.txt', 'r')
		f = open('list_remove_chrome.txt', 'r')
		line = f.readline()
		while line != '':
			target, list_ = line.split('   ', 1)
			dependents_nonstrip = list_.strip().split(',')
			#print 'target:', target
			#print 'dependents', dependents
			
			dependents = []
			for a in dependents_nonstrip:
				dependents.append(a.strip())
			dep_list[target] = dependents
			
			line = f.readline()
		#end while
		f.close()
		# <<< load dependency info <<<
	#end if len(dep_list)
	
	if DEBUG_PRINT_DEP_LIST: print '>>>> print dep_list >>>>'
	for key, value in dep_list.items():
		if (root_target is None) and (target_name in key):
			if DEBUG: print 'found target ', target_name, ', key ', key
			root_target = Dep(key);
		if DEBUG_PRINT_DEP_LIST: print key, value
	if DEBUG_PRINT_DEP_LIST: print '<<<< print dep_list <<<<'

	if root_target is None:
		print 'ERROR: Cannnot find root target ', target_name
		return
		
	#for a in dep_list: #debug
	#	print a.getTarget()
	#	print a.getDep()
	
	cur_target = root_target
	# Create modules that we want to search
	
	search_modules = []
	search_modules.append(root_target.getTarget())
	
	#==========================================  search  ==========================================
	#debug
	if DEBUG: print 'Before search: len(search_modules):', len(search_modules)
	while True:
		for module in search_modules:
			if DEBUG: print 'search_module:' + module
			
			cur_target = findNodeInTree(module, root_target)
			if cur_target is None:
				print 'ERROR: cannot find ', module
				break
			found_dep_list = findDepList(cur_target.getTarget(), dep_list)
			
			# Add newly found modules to search_modules
			for x in found_dep_list:
				# [CyclicCheck] If the module was already found, do not add to the modules
				if findNodeInTree(x.getTarget(), root_target) == None:
					if DEBUG: print 'search: append:', x.getTarget()
					search_modules.append(x.getTarget())
	
			# add found dep list here for CyclicCheck
			cur_target.setDep(found_dep_list)
			
			#remove the current module from search_modules
			search_modules.remove(module)
			
			#debug
			if DEBUG: print 'after search: len(search_modules):', len(search_modules)
		#end for module in search_modules:
		
		if len(search_modules) == 0:
			if DEBUG: print 'no more search_modules'
			break
	#end while true
	
	# print tree
	if saveToFile:
		fout = open('tree.txt', 'w')
		printTree(root_target, saveToFile, fout)
		fout.close()
	else:
		printTree(root_target)
