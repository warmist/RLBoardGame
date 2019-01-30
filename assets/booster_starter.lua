function next_cell( pos,dir )
	local dir2=dir/math.max(math.abs(dir[1]),math.abs(dir[2]))
	return pos+dir2
end
deck={}
print("Actually running lua code!")
deck.strike={
	--main
	name="Strike",
	cost=2,
	description="Perform an attack with a very big sword",
	--misc
	attack=3,
	range=2,
	--callbacks
	use=function ( card,game )
		local target=game.target_enemy(game.player:pos(),card.range)
		game.damage(target,card.attack)
	end
}
deck.push={

	name="Push",
	cost=1,
	description="Force a move. Deal damage if can't move",

	attack=1,
	range=2,
	distance=3,

	use=function ( card,game )
		--first ask which one of enemies you want to push
		local target=game.target_enemy(game.player:pos(),card.range)
		--by default if the first "target" function is canceled. the whole
		--		card is not played and returned to the hand

		--figure out the direction of push
		local dir=target:pos()-game.player:pos()
		dir=dir:normalized()
		local start=target:pos()

		--raycast (i.e. like going straight in that direction)
		local target_cell=start+dir*card.distance
		local path=game.raycast(next_cell(start,dir),target_cell)
		--move the unit
		game.move(target,path)
		
		--then if the unit could not move ALL the way, hurt it
		if #path<card.distance-1 then
			game.damage(target,card.attack)
		end
	end
}
deck.move={
	name="Run",
	cost=1,
	description="Run to the target",

	generated=true,
	range=5,
	move=true,

	use=function (card,game)
		local path=game.target_path(game.player:pos(),card.range)
		game.move(game.player,path)
		return "destroy"
	end
}
return deck