function v = helics_state_execution()
  persistent vInitialized;
  if isempty(vInitialized)
    vInitialized = helicsMEX(0, 1432107609);
  end
  v = vInitialized;
end
