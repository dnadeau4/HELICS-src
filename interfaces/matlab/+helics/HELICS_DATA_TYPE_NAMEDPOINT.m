function v = HELICS_DATA_TYPE_NAMEDPOINT()
  persistent vInitialized;
  if isempty(vInitialized)
    vInitialized = helicsMEX(0, 1432107664);
  end
  v = vInitialized;
end
