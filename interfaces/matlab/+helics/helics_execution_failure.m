function v = helics_execution_failure()
  persistent vInitialized;
  if isempty(vInitialized)
    vInitialized = helicsMEX(0, 1398230862);
  end
  v = vInitialized;
end