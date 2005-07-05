package client;

import common.*;

public class PerfExplorerModel extends RMIPerfExplorerModel {
	private static PerfExplorerModel theModel = null;

	public static PerfExplorerModel getModel() {
		if (theModel == null) {
			theModel = new PerfExplorerModel();
		}
		return theModel;
	}

	public RMIPerfExplorerModel copy() {
		return new RMIPerfExplorerModel(theModel);
	}

}
